/*
 * cairo.hh
 * Copyright (C) 2017 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef CAIRO_HH
#define CAIRO_HH

#include <cairo.h>
#include <glib.h>
#include <cmath>


namespace cairo {
    constexpr static const char* name = "cairo";

    using Status = ::cairo_status_t;
    using Format = ::cairo_format_t;

    namespace format {
        constexpr Format ARGB32 = CAIRO_FORMAT_ARGB32;
        constexpr Format RGB16_565 = CAIRO_FORMAT_RGB16_565;
    };

    template <typename T,
              Status (*do_get_status)(T*),
              void (*do_destroy)(T*)>
    class Ref {
    public:
        using Type = T;

        ~Ref() {
            if (m_pointer) {
                do_destroy(m_pointer);
                m_pointer = nullptr;
            }
        }

        inline Type* pointer() { return m_pointer; }
        inline const Type* constPointer() const { return m_pointer; }
        inline Status status() const { return do_get_status(m_pointer); }
        inline const char* statusString() const { return ::cairo_status_to_string(status()); }
        inline operator bool() const { return status() == CAIRO_STATUS_SUCCESS; }

    protected:
        explicit Ref(Type* pointer) : m_pointer(pointer) {
            g_return_if_fail(m_pointer != nullptr);
            g_return_if_fail(status() == CAIRO_STATUS_SUCCESS);
        }

        Ref(const Ref&) = delete;
        void operator=(const Ref&) = delete;

    private:
        Type* m_pointer;
    };


    class Surface : public Ref<::cairo_surface_t,
                               ::cairo_surface_status,
                               ::cairo_surface_destroy> {
    public:
        Surface(Ref::Type* surface) : Ref(surface) { }
        Surface(Format format,
                void* bits,
                uint32_t width,
                uint32_t height,
                uint32_t stride)
              : Ref(::cairo_image_surface_create_for_data(static_cast<unsigned char*>(bits),
                                                          format,
                                                          width,
                                                          height,
                                                          stride)) { }

        inline uint32_t width() const {
            auto value = ::cairo_image_surface_get_width(const_cast<Type*>(constPointer()));
            return (value < 0) ? 0 : static_cast<uint32_t>(value);
        }
        inline uint32_t height() const {
            auto value = ::cairo_image_surface_get_height(const_cast<Type*>(constPointer()));
            return (value < 0) ? 0 : static_cast<uint32_t>(value);
        }
    };


    enum Rotation {
        None = 0,
        ClockWise90,
        ClockWise180,
        ClockWise270,
        ClockWise360 = None,
        CounterClockWise90 = ClockWise270,
        CounterClockWise180 = ClockWise180,
        CounterClockWise270 = ClockWise90,
        CounterClockWise360 = None,
    };


    class Context : public Ref<::cairo_t,
                               ::cairo_status,
                               ::cairo_destroy> {
    public:
        explicit Context(Surface& surface) : Ref(cairo_create(surface.pointer())) { }

        inline Context& source(Surface& surface, double x = 0.0, double y = 0.0) {
            ::cairo_set_source_surface(pointer(), surface.pointer(), x, y);
            return *this;
        }

        inline Context& rotate(const Surface& surface, Rotation angle) {
            switch (angle) {
                case Rotation::ClockWise90:
                    ::cairo_translate(pointer(), surface.height(), 0);
                    ::cairo_rotate(pointer(), 90.0 * M_PI / 180.0);
                    break;
                case Rotation::ClockWise180:
                    ::cairo_translate(pointer(), surface.width(), surface.height());
                    ::cairo_rotate(pointer(), 180.0 * M_PI / 180.0);
                    break;
                case Rotation::ClockWise270:
                    ::cairo_translate(pointer(), 0, surface.width());
                    ::cairo_rotate(pointer(), 270.0 * M_PI / 180.0);
                    break;
                default:  // No rotation.
                    break;
            }
            return *this;
        }

        inline Context& paint() {
            ::cairo_paint(pointer());
            return *this;
        }
    };

} // namespace cairo

#endif /* !CAIRO_HH */
