/*
 * pixman.hh
 * Copyright (C) 2017 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef PIXMAN_HH
#define PIXMAN_HH

#include <pixman.h>
#include <cmath>

namespace pixman {
    constexpr static const char* name = "pixman";

    using Format = ::pixman_format_code_t;

    namespace format {
        constexpr Format ARGB32 = PIXMAN_a8r8g8b8;
        constexpr Format RGB16_565 = PIXMAN_r5g6b5;
    };

    template <typename T, typename R, R (*do_destroy)(T*)>
    class Ref {
    public:
        constexpr Ref(Ref&& other) = default;

        ~Ref() {
            if (m_pointer) {
                do_destroy(m_pointer);
                m_pointer = nullptr;
            }
        }

        const T* constPointer() const { return m_pointer; }
        T* pointer() { return m_pointer; }

    protected:
        explicit Ref(T* pointer) : m_pointer(pointer) { }

    private:
        Ref(const Ref&) = delete;
        void operator=(const Ref&) = delete;

        T* m_pointer;
    };

    class Transform;

    class Surface : public Ref<pixman_image_t,
                               pixman_bool_t,
                               pixman_image_unref> {
    public:
        Surface(Format format,
                void* bits,
                uint32_t width,
                uint32_t height,
                uint32_t stride)
              : Ref(::pixman_image_create_bits_no_clear(format,
                                                        static_cast<int>(width),
                                                        static_cast<int>(height),
                                                        static_cast<uint32_t*>(bits),
                                                        static_cast<int>(stride)))
        {
        }

        inline uint32_t width() const {
            return ::pixman_image_get_width(const_cast<::pixman_image_t*>(constPointer()));
        }
        inline uint32_t height() const {
            return ::pixman_image_get_height(const_cast<::pixman_image_t*>(constPointer()));
        }

        void setTransform(const Transform&);
    };


    class Transform {
    public:
        static Transform identity() {
            Transform xfrm;
            ::pixman_f_transform_init_identity(&xfrm.m_transform);
            return xfrm;
        }

        static Transform rotate(double c, double s) {
            Transform xfrm;
            ::pixman_f_transform_init_rotate(&xfrm.m_transform, c, s);
            return xfrm;
        }

        inline static Transform rotate(double d) {
            return rotate(std::cos(d), std::sin(d));
        }

    private:
        Transform() = default;

        ::pixman_transform_t asFixed() const {
            ::pixman_transform_t xfrm;
            pixman_transform_from_pixman_f_transform(&xfrm, &m_transform);
            return xfrm;
        }

        ::pixman_f_transform_t m_transform;

        friend class Surface;
    };

    void Surface::setTransform(const Transform& xfrm) {
        const auto fx = xfrm.asFixed();
        ::pixman_image_set_transform(pointer(), &fx);
    }
};

#endif /* !PIXMAN_HH */
