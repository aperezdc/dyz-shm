#include <glib.h>
#include <wpe-fdo/view-backend-exportable.h>

#define BUILDING_WPE__

#include <WebKit/WKContext.h>
#include <WebKit/WKPage.h>
#include <WebKit/WKPageGroup.h>
#include <WebKit/WKPageConfigurationRef.h>
#include <WebKit/WKString.h>
#include <WebKit/WKType.h>
#include <WebKit/WKURL.h>
#include <WebKit/WKView.h>

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cairo.h>
#include <inttypes.h>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <cstring>
#include <cerrno>


struct Surface {
public:
    Surface(cairo_surface_t* surface) : m_surface(surface) {
        g_return_if_fail(m_surface != nullptr);
    }

    ~Surface() {
        cairo_surface_destroy(m_surface);
        m_surface = nullptr;
    }

    inline cairo_surface_t* get() { return m_surface; }
    inline operator bool() const { return status() != CAIRO_STATUS_SUCCESS; }
    inline cairo_status_t status() const { return cairo_surface_status(m_surface); }

private:
    Surface(const Surface&) = delete;
    void operator=(const Surface&) = delete;

    cairo_surface_t* m_surface { nullptr };
};


struct Cairo {
public:
    explicit Cairo(Surface& surface) : m_ctx(cairo_create(surface.get())) {
        g_return_if_fail(status() != CAIRO_STATUS_SUCCESS);
    }

    ~Cairo() {
        cairo_destroy(m_ctx);
        m_ctx = nullptr;
    }

    inline cairo_t* get() { return m_ctx; }
    inline operator bool() const { return status() != CAIRO_STATUS_SUCCESS; }
    inline cairo_status_t status() const { return cairo_status(m_ctx); }

    inline void setSource(Surface& surface, double x = 0.0, double y = 0.0) {
        cairo_set_source_surface(m_ctx, surface.get(), x, y);
    }
    inline void paint() {
        cairo_paint(m_ctx);
    }

private:
    Cairo(const Cairo&) = delete;
    void operator=(const Cairo&) = delete;

    cairo_t* m_ctx { nullptr };
};


struct FrameBuffer {
public:
    FrameBuffer(const char* devicePath = nullptr) : m_devicePath(devicePath) {
        if (!m_devicePath) {
            if (!(m_devicePath = getenv("WPE_FBDEV"))) {
                m_devicePath = "/dev/fb0";
            }
        }

        do {
            m_fd = open(m_devicePath, O_RDWR);
        } while (m_fd == -1 && errno == EINTR);
        if (m_fd == -1 ) {
            markError("open", errno);
            return;
        }

        if (!updateScreenInfo())
            return;

        int retcode;
        do {
            retcode = ioctl(m_fd, FBIOBLANK, FB_BLANK_UNBLANK);
        } while (retcode == -1 && errno == EINTR);
        if (retcode == -1) {
            markError("ioctl FBIOBLANK FB_BLANK_UNBLANK", errno);
            return;
        }

        if (size() > m_fixInfo.smem_len) {
            markError("mmap", "size to mmap bigger than framebuffer size");
            return;
        }

        if ((m_buffer = mmap(nullptr, size(), PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0)) == MAP_FAILED) {
            markError("mmap", errno);
            return;
        }

        if (!createSurface()) {
            markError("cairo", "Cannot create device surface");
            return;
        }
    }

    bool updateScreenInfo() {
        int retcode;
        do {
            retcode = ioctl(m_fd, FBIOGET_FSCREENINFO, &m_fixInfo);
        } while (retcode == -1 && errno == EINTR);
        if (retcode == -1) {
            markError("ioctl FBIOGET_FSCREENINFO", errno);
            return false;
        }

        do {
            retcode = ioctl(m_fd, FBIOGET_VSCREENINFO, &m_varInfo);
        } while (retcode == -1 && errno == EINTR);
        if (retcode == -1) {
            markError("ioctl FBIOGET_VSCREENINFO", errno);
            return false;
        }
    }

    ~FrameBuffer() {
        delete m_surface;

        if (m_buffer) {
            munmap(m_buffer, size());
            m_buffer = nullptr;
        }

        if (m_fd != -1) {
            close(m_fd);
            m_fd = -1;
        }
    }

    inline void* data() { return m_buffer; }
    inline const void* constData() const { return m_buffer; }
    inline uint32_t stride() const { return m_fixInfo.line_length; }
    inline uint64_t size() const { return stride() * yres(); }
    inline uint32_t xres() const { return m_varInfo.xres; }
    inline uint32_t yres() const { return m_varInfo.yres; }
    inline uint32_t bpp() const { return m_varInfo.bits_per_pixel; }

    inline operator bool() const { return m_fd != -1 && !errored(); }
    inline bool errored() const { return !!m_errorMessage; }
    inline const char* errorMessage() const { return m_errorMessage; }
    inline const char* errorCause() const { return m_errorCause; }
    inline const char* devicePath() const { return m_devicePath; }
    inline Surface& surface() {
        g_assert(m_surface != nullptr);
        return *m_surface;
    }

protected:
    FrameBuffer(const FrameBuffer&) = delete; // Prevent copying;
    void operator=(const FrameBuffer&) = delete; // Prevent assignment.

    void markError(const char* cause, const char* message) {
        m_errorCause = cause;
        m_errorMessage = message;
    }
    inline void markError(const char* cause, int err) {
        markError(cause, strerror(err));
    }

    bool createSurface() {
        m_surface = new Surface(cairo_image_surface_create_for_data(static_cast<unsigned char*>(m_buffer),
                                                                    CAIRO_FORMAT_RGB16_565,
                                                                    xres(),
                                                                    yres(),
                                                                    stride()));
        return m_surface->status() == CAIRO_STATUS_SUCCESS;
    }

private:
    int m_fd { -1 };
    void* m_buffer { nullptr };
    const char* m_errorMessage { nullptr };
    const char* m_errorCause { nullptr };
    struct fb_var_screeninfo m_varInfo { };
    struct fb_fix_screeninfo m_fixInfo { };
    const char* m_devicePath;
    Surface* m_surface { nullptr };
};



struct ViewData {
    FrameBuffer& framebuffer;
    struct wpe_view_backend_exportable_shm* exportable;
};


static struct wpe_view_backend_exportable_shm_client s_exportableSHMClient = {
    // export_buffer
    [](void* data, struct wpe_view_backend_exportable_shm_buffer* buffer)
    {
        g_printerr("export_buffer() %p\n",
                   buffer);
        g_printerr("  buffer_resource %p buffer %p\n",
                   buffer->buffer_resource,
                   buffer->buffer);
        g_printerr("  data %p (%d,%d) stride %d\n",
                   buffer->data,
                   buffer->width,
                   buffer->height,
                   buffer->stride);

        Surface image { cairo_image_surface_create_for_data(static_cast<unsigned char*>(buffer->data),
                                                            CAIRO_FORMAT_ARGB32,
                                                            buffer->width,
                                                            buffer->height,
                                                            buffer->stride) };
        if (!image) {
            g_printerr("Could not create cairo surface for SHM buffer\n");
            return;
        }

        const char* png_image_path = getenv("WPE_DUMP_PNG_PATH");
        if (png_image_path) {
            char filename[128];
            static int files = 0;
            sprintf(filename, "%sdump_%d.png", png_image_path, files++);
            cairo_surface_write_to_png(image.get(), filename);
            g_printerr("dump image data to %s\n", png_image_path);
        }

        auto* viewData = reinterpret_cast<ViewData*>(data);
        Cairo context { viewData->framebuffer.surface() };
        context.setSource(image);
        context.paint();

        wpe_view_backend_exportable_shm_dispatch_frame_complete(viewData->exportable);
        wpe_view_backend_exportable_shm_dispatch_release_buffer(viewData->exportable, buffer);
    },
};

int main()
{
    FrameBuffer framebuffer;
    if (!framebuffer) {
        g_printerr("Cannot initialize framebuffer: %s (%s)\n",
                   framebuffer.errorMessage(),
                   framebuffer.errorCause());
        return EXIT_FAILURE;
    }

    g_printerr("Framebuffer '%s' @ %" PRIu32 "x%" PRIu32 " %" PRIu32 "bpp"
               " (stride %" PRIu32 ", size %" PRIu64 ", %p)\n",
               framebuffer.devicePath(),
               framebuffer.xres(),
               framebuffer.yres(),
               framebuffer.bpp(),
               framebuffer.stride(),
               framebuffer.size(),
               framebuffer.constData());

    GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);

    auto context = WKContextCreate();
    auto pageConfiguration = WKPageConfigurationCreate();

    {
        auto pageGroupIdentifier = WKStringCreateWithUTF8CString("WPEPageGroup");
        auto pageGroup = WKPageGroupCreateWithIdentifier(pageGroupIdentifier);

        WKPageConfigurationSetContext(pageConfiguration, context);
        WKPageConfigurationSetPageGroup(pageConfiguration, pageGroup);

        WKRelease(pageGroup);
        WKRelease(pageGroupIdentifier);
    }

    ViewData viewData { framebuffer, nullptr };
    auto* backendExportable = wpe_view_backend_exportable_shm_create(&s_exportableSHMClient, &viewData);
    viewData.exportable = backendExportable;

    auto* backend = wpe_view_backend_exportable_shm_get_view_backend(backendExportable);
    auto view = WKViewCreateWithViewBackend(backend, pageConfiguration);

    {
        auto shellURL = WKURLCreateWithUTF8CString("http://localhost:3000");
        WKPageLoadURL(WKViewGetPage(view), shellURL);
        WKRelease(shellURL);
    }

    g_main_loop_run(loop);

    WKRelease(view);

    wpe_view_backend_exportable_shm_destroy(backendExportable);

    WKRelease(pageConfiguration);
    WKRelease(context);

    g_main_loop_unref(loop);
    return EXIT_SUCCESS;
}
