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

#include <cairo.h>
#include <cstdio>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static const int WIDTH = 320;
static const int HEIGHT = 240;

cairo_surface_t *g_framebuffer_surface;
cairo_t *g_framebuffer_cr;

typedef struct _cairo_fb_device {
    int fd;
    unsigned char *fbp;
    long screensize;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
} cairo_fb_device_t;

struct ViewData {
    struct wpe_view_backend_exportable_shm* exportable;
};


static void closeToUseFrameBuffer(void *device) {
    cairo_fb_device_t *dev = static_cast<cairo_fb_device_t *>(device);

    if (dev == nullptr)
        return;

    munmap(dev->fbp, dev->screensize);
    close(dev->fd);
    free(dev);

    cairo_destroy(g_framebuffer_cr);
    cairo_surface_destroy(g_framebuffer_surface);
}

int prepareToUseFrameBuffer(int width, int height) {
    cairo_fb_device_t *device;

    char* fb_driver_name = getenv("WPE_FBDEV");
    if (!fb_driver_name)
        fb_driver_name = "/dev/fb0";

    device = static_cast<cairo_fb_device_t*>(malloc(sizeof(*device)));

    device->fd = open(fb_driver_name, O_RDWR);
    if (device->fd == -1) {
        fprintf(stderr, "Error: cannot open framebuffer device");
        return -1;
    }
    fprintf(stderr, "The framebuffer device was opened successfully.\n");

    if (ioctl(device->fd, FBIOGET_FSCREENINFO, &device->finfo) == -1) {
        fprintf(stderr, "Error reading fixed information");
        return -1;
    }

    if (ioctl(device->fd, FBIOGET_VSCREENINFO, &device->vinfo) == -1) {
        fprintf(stderr, "Error reading variable information");
        return -1;
    }

    if (ioctl (device->fd, FBIOBLANK, FB_BLANK_UNBLANK) == -1) {
        fprintf(stderr, "Error wake up the display");
        return -1;
    }

    fprintf(stderr, "%dx%d, %dbpp\n", width, height, device->vinfo.bits_per_pixel);

    // Figure out the size of the screen in bytes
    device->screensize = width * height * device->vinfo.bits_per_pixel / 8;

    // Map the device to memory
    device->fbp = static_cast<unsigned char*>(mmap(0, device->screensize, PROT_READ | PROT_WRITE, MAP_SHARED, device->fd, 0));
    if (!device->fbp) {
        fprintf(stderr, "Error: failed to map framebuffer device to memory");
        return -1;
    }
    fprintf(stderr, "The framebuffer device was mapped to memory successfully.\n");

    g_framebuffer_surface = cairo_image_surface_create_for_data(device->fbp,
                                                                CAIRO_FORMAT_ARGB32,
                                                                device->vinfo.xres,
                                                                device->vinfo.yres,
                                                                cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32,
                                                                device->vinfo.xres));
    cairo_surface_set_user_data(g_framebuffer_surface, nullptr, device, &closeToUseFrameBuffer);

    g_framebuffer_cr = cairo_create(g_framebuffer_surface);

    return 0;
}

int emitImageDataToFrameBuffer(cairo_surface_t* image, int image_width, int image_height) {
    cairo_set_source_surface(g_framebuffer_cr, image, image_width, image_height);
    cairo_paint(g_framebuffer_cr);
    return 0;
}

static struct wpe_view_backend_exportable_shm_client s_exportableSHMClient = {
    // export_buffer
    [](void* data, struct wpe_view_backend_exportable_shm_buffer* buffer)
    {
        auto* viewData = reinterpret_cast<ViewData*>(data);

        fprintf(stderr, "export_buffer() %p\n", buffer);
        fprintf(stderr, "  buffer_resource %p buffer %p\n",
            buffer->buffer_resource, buffer->buffer);
        fprintf(stderr, "  data %p (%d,%d) stride %d\n",
            buffer->data, buffer->width, buffer->height, buffer->stride);

        cairo_surface_t* image_surface = cairo_image_surface_create_for_data(static_cast<unsigned char*>(buffer->data),
                                                                           CAIRO_FORMAT_ARGB32,
                                                                           buffer->width,
                                                                           buffer->height,
                                                                           buffer->stride);

        char* png_image_path = getenv("WPE_DUMP_PNG_PATH");
        fprintf(stderr, "png_image_path = %s\n", png_image_path);
        if (png_image_path) {
            char filename[128];
            static int files = 0;
            sprintf(filename, "%sdump_%d.png", png_image_path, files++);
            cairo_surface_write_to_png(image_surface, filename);
            fprintf(stderr, "dump image data to %s\n", png_image_path);
        }
        emitImageDataToFrameBuffer(image_surface, buffer->width, buffer->height);

        cairo_surface_destroy(image_surface);

        wpe_view_backend_exportable_shm_dispatch_frame_complete(viewData->exportable);
        wpe_view_backend_exportable_shm_dispatch_release_buffer(viewData->exportable, buffer);
    },
};

int main()
{
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

    auto* viewData = new ViewData;
    auto* backendExportable = wpe_view_backend_exportable_shm_create(&s_exportableSHMClient, viewData);
    viewData->exportable = backendExportable;

    auto* backend = wpe_view_backend_exportable_shm_get_view_backend(backendExportable);
    auto view = WKViewCreateWithViewBackend(backend, pageConfiguration);

    prepareToUseFrameBuffer(WIDTH, HEIGHT);

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
    return 0;
}
