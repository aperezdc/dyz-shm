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


namespace cairo {

    using Status = ::cairo_status_t;

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

        inline Type* get() { return m_pointer; }
        inline Status status() const { return do_get_status(m_pointer); }
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
    };


    class Context : public Ref<::cairo_t,
                               ::cairo_status,
                               ::cairo_destroy> {
    public:
        explicit Context(Surface& surface) : Ref(cairo_create(surface.get())) { }

        inline void setSource(Surface& surface, double x = 0.0, double y = 0.0) {
            cairo_set_source_surface(get(), surface.get(), x, y);
        }
        inline void paint() { cairo_paint(get()); }
    };

} // namespace cairo

#endif /* !CAIRO_HH */
