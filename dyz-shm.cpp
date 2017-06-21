#include <glib.h>
#include <wpe-fdo/view-backend-exportable.h>

#include <WPE/WebKit.h>

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <inttypes.h>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <utility>
#include <cstring>
#include <cerrno>

#if GRAPHICS_CAIRO
# include "cairo.hh"
#elif GRAPHICS_PIXMAN
# include "pixman.hh"
#elif GRAPHICS_SIMPLE
# include "simplegfx.hh"
#else
# error No graphics backend
#endif


static bool  sDebug = false;
static char* sPNGPath = nullptr;

#define DEBUG(args) \
    do { \
        if (sDebug) { g_printerr args ; } \
    } while (0)


struct FrameBuffer {
public:
#if GRAPHICS_CAIRO
    using SurfaceType = cairo::Surface;
    static constexpr const char* imageBackend { "cairo" };
#elif GRAPHICS_PIXMAN
    using SurfaceType = pixman::Image;
    static constexpr const char* imageBackend { "pixman" };
#elif GRAPHICS_SIMPLE
    static constexpr const char* imageBackend { "simple" };
#endif

    FrameBuffer(const char* devicePath = nullptr) : m_devicePath(devicePath) {
        if (!m_devicePath) {
            if (auto value = getenv("WPE_FBDEV")) {
                m_devicePath = value;
            } else {
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
        DEBUG(("Framebuffer '%s' fd: %i\n", m_devicePath, m_fd));

        if (!updateScreenInfo())
            return;
        DEBUG(("Framebuffer '%s' smem_len = %" PRIu32 "\n",
               m_devicePath, m_fixInfo.smem_len));

        int retcode;
        do {
            retcode = ioctl(m_fd, FBIOBLANK, FB_BLANK_UNBLANK);
        } while (retcode < 0 && errno == EINTR);
        if (retcode < 0) {
            markError("ioctl FBIOBLANK FB_BLANK_UNBLANK", errno);
            return;
        }
        DEBUG(("Framebuffer '%s' unblanked\n", m_devicePath));

        if (size() > m_fixInfo.smem_len) {
            markError("mmap", "size to mmap bigger than framebuffer size");
            return;
        }

        m_buffer = mmap(nullptr, size(), PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
        if (m_buffer == MAP_FAILED || !m_buffer) {
            markError("mmap", errno);
            return;
        }

        if (!createSurface()) {
            markError(imageBackend, "Cannot create device surface");
            return;
        }
    }

    bool updateScreenInfo() {
        int retcode;
        do {
            retcode = ioctl(m_fd, FBIOGET_FSCREENINFO, &m_fixInfo);
        } while (retcode < 0 && errno == EINTR);
        if (retcode < 0) {
            markError("ioctl FBIOGET_FSCREENINFO", errno);
            return false;
        }

        do {
            retcode = ioctl(m_fd, FBIOGET_VSCREENINFO, &m_varInfo);
        } while (retcode < 0 && errno == EINTR);
        if (retcode < 0) {
            markError("ioctl FBIOGET_VSCREENINFO", errno);
            return false;
        }
        return true;
    }

    ~FrameBuffer() {
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
    inline uint32_t rotation() const { return m_varInfo.rotate; }

    bool setRotation(uint32_t rotation) {
        m_varInfo.rotate = rotation;
        return applyVarInfo();
    }

    inline bool errored() const { return m_errorCause || m_errorMessage; }
    inline const char* errorMessage() const { return m_errorMessage; }
    inline const char* errorCause() const { return m_errorCause; }
    inline const char* devicePath() const { return m_devicePath; }

protected:
    FrameBuffer(const FrameBuffer&) = delete; // Prevent copying;
    void operator=(const FrameBuffer&) = delete; // Prevent assignment.

    void markError(const char* cause, const char* message) {
        DEBUG(("Framebuffer error: %s (%s)\n", message, cause));
        m_errorCause = cause;
        m_errorMessage = message;
    }
    inline void markError(const char* cause, int err) {
        markError(cause, strerror(err));
    }

    bool createSurface() {
#if GRAPHICS_CAIRO
        m_surface.reset(new SurfaceType(
            ::cairo_image_surface_create_for_data(static_cast<unsigned char*>(m_buffer),
                                                  CAIRO_FORMAT_RGB16_565,
                                                  xres(),
                                                  yres(),
                                                  stride())));
        return m_surface->status() == CAIRO_STATUS_SUCCESS;
#elif GRAPHICS_PIXMAN
        m_surface.reset(new SurfaceType(PIXMAN_r5g6b5,
                                        xres(),
                                        yres(),
                                        m_buffer,
                                        stride()));
        return !!m_surface;
#elif GRAPHICS_SIMPLE
        return true;
#endif
    }

    bool applyVarInfo() {
        int retcode;
        do {
            retcode = ioctl(m_fd, FBIOPUT_VSCREENINFO, &m_varInfo);
        } while (retcode < 0 && errno == EINTR);
        return retcode >= 0;
    }

private:
    int m_fd { -1 };
    void* m_buffer { nullptr };
    const char* m_errorMessage { nullptr };
    const char* m_errorCause { nullptr };
    struct fb_var_screeninfo m_varInfo { };
    struct fb_fix_screeninfo m_fixInfo { };
    const char* m_devicePath;

#if !GRAPHICS_SIMPLE
    std::unique_ptr<SurfaceType> m_surface { nullptr };

public:
    inline SurfaceType& surface() {
        g_assert(m_surface != nullptr);
        return *m_surface;
    }
#endif
};



struct ViewData {
    FrameBuffer& framebuffer;
    struct wpe_view_backend_exportable_shm* exportable;
};


static struct wpe_view_backend_exportable_shm_client s_exportableSHMClient = {
    // export_buffer
    [](void* data, struct wpe_view_backend_exportable_shm_buffer* buffer)
    {
        DEBUG(("export_buffer() %p\n", buffer));
        DEBUG(("  buffer_resource %p buffer %p\n",
               buffer->buffer_resource,
               buffer->buffer));
        DEBUG(("  data %p (%d,%d) stride %d\n",
               buffer->data,
               buffer->width,
               buffer->height,
               buffer->stride));

        auto* viewData = reinterpret_cast<ViewData*>(data);

#if GRAPHICS_CAIRO
        cairo::Surface image { cairo_image_surface_create_for_data(static_cast<unsigned char*>(buffer->data),
                                                                   CAIRO_FORMAT_ARGB32,
                                                                   buffer->width,
                                                                   buffer->height,
                                                                   buffer->stride) };
        if (!image) {
            g_printerr("Could not create cairo surface for SHM buffer: %s\n", image.statusString());
            return;
        }

        if (sPNGPath) {
            char filename[PATH_MAX];
            static int files = 0;
            snprintf(filename, PATH_MAX, "%s/dump_%d.png", sPNGPath, files++);
            cairo_surface_write_to_png(image.get(), filename);
            g_printerr("dump image data to %s\n", filename);
        }

        cairo::Context context { viewData->framebuffer.surface() };
        context.rotate(image, cairo::Rotation::ClockWise90).source(image).paint();

#elif GRAPHICS_PIXMAN

        pixman::Image image {
            PIXMAN_a8r8g8b8,
            buffer->width,
            buffer->height,
            buffer->data,
            buffer->stride
        };

        image->setTransform(pixman::Transform::rotate(90));
        ::pixman_image_composite(PIXMAN_OP_SRC,
                                 image.pointer(),
                                 nullptr,
                                 viewData->framebuffer.surface().pointer(),
                                 0, 0,
                                 0, 0,
                                 0, 0,
                                 image.width(),
                                 image.height());
#elif GRAPHICS_SIMPLE
        auto fbLines = viewData->framebuffer.yres();
        auto fbColumns = viewData->framebuffer.xres();
        auto fbStride = viewData->framebuffer.stride();
        uint8_t* fbData = reinterpret_cast<uint8_t*>(viewData->framebuffer.data());
        uint8_t* bufData = reinterpret_cast<uint8_t*>(buffer->data);

        for (auto fbY = 0; fbY < fbLines; fbY++) {
            uint16_t *fbLineData = reinterpret_cast<uint16_t*>(fbData + fbStride * fbY);
            for (auto fbX = 0; fbX < fbColumns; fbX++) {
                fbLineData[fbX] = simplegfx::Argb32toRgb565_v0(
                    reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(buffer->data) + buffer->stride * fbX)[fbLines - fbY]);
            }
        }
#endif

        wpe_view_backend_exportable_shm_dispatch_frame_complete(viewData->exportable);
        wpe_view_backend_exportable_shm_dispatch_release_buffer(viewData->exportable, buffer);
    },
};

int main(int argc, char *argv[])
{
    if (auto value = getenv("WPE_DYZSHM_DEBUG")) {
        sDebug = strcmp(value, "0") != 0;
    }
    if (auto value = getenv("WPE_DUMP_PNG_PATH")) {
        sPNGPath = value;
    }

    FrameBuffer framebuffer;
    if (framebuffer.errored()) {
        g_printerr("Cannot initialize framebuffer: %s (%s)\n",
                   framebuffer.errorMessage(),
                   framebuffer.errorCause());
        return EXIT_FAILURE;
    }

    DEBUG(("Framebuffer '%s' @ %" PRIu32 "x%" PRIu32 " %" PRIu32 "bpp"
           " (%" PRIu32 ", stride %" PRIu32 ", size %" PRIu64 ", %p)\n",
           framebuffer.devicePath(),
           framebuffer.xres(),
           framebuffer.yres(),
           framebuffer.bpp(),
           framebuffer.rotation(),
           framebuffer.stride(),
           framebuffer.size(),
           framebuffer.constData()));

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
        auto shellURL = WKURLCreateWithUTF8CString((argc > 1) ? argv[1] : "http://igalia.com");
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
