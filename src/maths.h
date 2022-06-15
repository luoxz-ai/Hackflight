/*
   Copyright (c) 2022 Simon D. Levy

   This file is part of Hackflight.

   Hackflight is free software: you can redistribute it and/or modify it under the
   terms of the GNU General Public License as published by the Free Software
   Foundation, either version 3 of the License, or (at your option) any later
   version.

   Hackflight is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
   PARTICULAR PURPOSE. See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with
   Hackflight. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <math.h>
#include <stdint.h>

typedef struct fp_rotationMatrix_s {
    float m[3][3];              // matrix
} fp_rotationMatrix_t;

#if defined(__cplusplus)
extern "C" {
#endif

    float sin_approx(float x);
    float cos_approx(float x);

    static inline int constrain_f_i32(float amt, int32_t low, int32_t high)
    {
        if (amt < low)
            return low;
        else if (amt > high)
            return high;
        else
            return amt;
    }

    static inline int constrain_u16_u16(uint16_t amt, uint16_t low, uint16_t high)
    {
        if (amt < low)
            return low;
        else if (amt > high)
            return high;
        else
            return amt;
    }

    static inline int constrain_i32_u32(int32_t amt, uint32_t low, uint32_t high)
    {
        if (amt < (int32_t)low)
            return low;
        else if (amt > (int32_t)high)
            return high;
        else
            return amt;
    }

    static inline float constrain_f(float amt, float low, float high)
    {
        if (amt < low)
            return low;
        else if (amt > high)
            return high;
        else
            return amt;
    }

#if defined(__cplusplus)
}
#endif
