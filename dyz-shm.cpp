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
#include <stdlib.h>

struct ViewData {
    struct wpe_view_backend_exportable_shm* exportable;
};

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

        char* png_image_path = getenv("WPE_DUMP_PNG_PATH");
        fprintf(stderr, "png_image_path = %s\n", png_image_path);
        if (png_image_path) {
            char filename[128];
            static int files = 0;
            cairo_surface_t* surface = cairo_image_surface_create_for_data(static_cast<unsigned char*>(buffer->data),
                                                                           CAIRO_FORMAT_ARGB32,
                                                                           buffer->width,
                                                                           buffer->height,
                                                                           buffer->stride);
            sprintf(filename, "%sdump_%d.png", png_image_path, files++);
            cairo_surface_write_to_png(surface, filename);
            fprintf(stderr, "dump image data to %s\n", png_image_path);
            cairo_surface_destroy(surface);
        }

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

    {
        auto shellURL = WKURLCreateWithUTF8CString("http://www.google.com");
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
