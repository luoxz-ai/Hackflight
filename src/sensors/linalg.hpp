/*
   linalg.hpp : Simple linear algebra support
   
   Copyright (c) 2018 Simon D. Levy

   This file is part of Hackflight.

   Hackflight is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Hackflight is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with Hackflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

namespace hf {

    class Matrix {

        private:

            // avoid dynamic memory allocation
            static const uint8_t MAXSIZE = 10;

            uint8_t _rows;
            uint8_t _cols;

            float _vals[MAXSIZE*MAXSIZE];

        public:

            Matrix(uint8_t rows, uint8_t cols, float * vals) 
            {
                _rows = rows;
                _cols = cols;
                memcpy(_vals, vals, rows*cols*sizeof(float));
            }

    };  // class Matrix

} // namespace hf
