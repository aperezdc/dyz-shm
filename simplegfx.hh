/*
 * simplegfx.hh
 * Copyright (C) 2017 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef SIMPLEGFX_HH
#define SIMPLEGFX_HH

#include <cstdint>

namespace simplegfx {
    constexpr static const char* name = "simplegfx";

    static inline uint16_t Argb32toRgb565_v0(uint32_t argb) {
        return static_cast<uint16_t>((((argb >> 19) & 0x1F) << 11) |
                                     (((argb >> 10) & 0x3F) <<  5) |
                                     (((argb >>  3) & 0x1F) <<  0));
    }

} // namespace simplegfx

#endif /* !SIMPLEGFX_HH */
