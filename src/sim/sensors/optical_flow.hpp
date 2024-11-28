/*
 *   Simulated optical flow sensor
 *
 *
 *   Copyright (C) 2024 Simon D. Levy
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, in version 3.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http:--www.gnu.org/licenses/>.
 */

#include <hackflight.hpp>
#include <sim/sensors/rangefinder.hpp>
#include <utils.hpp>

namespace hf {

    class OpticalFlow {

        public:

            // https://github.com/bitcraze/Bitcraze_PMW3901
            static constexpr float NPIX = 35;

            static float thetapix()
            {
                return 2 * sin(FIELD_OF_VIEW / Utils::RAD2DEG / 2);
            }

            static axis2_t read(const Dynamics & d, const float dt) 
            {
                // Rotate inertial-frame horizontal velocity into body frame
                const auto dx = d.x2 * cos(d.x11) - d.x4 * sin(d.x11);
                const auto dy = d.x2 * sin(d.x11) + d.x4 * cos(d.x11);

                (void)dt;
                const float scale = 22.5;

                const auto flow_dx = dx * scale;

                const auto flow_dy = dy * scale;

                return axis2_t {flow_dx, flow_dy};
            }

        private:

            // https://wiki.bitcraze.io/_media/projects:crazyflie2:
            //    expansionboards:pot0189-pmw3901mb-txqt-ds-r1.40-280119.pdf
            static constexpr float FIELD_OF_VIEW = 42;

            static constexpr float ZMIN = 0.1;

            static float max(const float a, const float b)
            {
                return a > b ? a : b;
            }
    };

}
