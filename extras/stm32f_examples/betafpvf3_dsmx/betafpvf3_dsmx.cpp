/*
   Sketch for Furious BetaFPV F3 brushed board with Spektrum DSMX receiver

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

#include <hackflight.hpp>
#include <mixers/quadxcf.hpp>
#include <pidcontrollers/level.hpp>
#include <boards/stm32f/betafpvf3.hpp>
#include <receivers/stm32f/dsmx.hpp>

static constexpr uint8_t CHANNEL_MAP[6] = {0, 1, 2, 3, 6, 4};

static constexpr float TRIM_ROLL  =  0.0f;
static constexpr float TRIM_PITCH = +0.15;
static constexpr float TRIM_YAW   =  0.0f;

static hf::Hackflight h;

extern "C" {

    void setup(void)
    {
        hf::Rate * ratePid = new hf::Rate(
                0.125f,    // Gyro cyclic P
                0.001875f, // Gyro cyclic I
                0.175f,    // Gyro cyclic D
                0.625f,    // Gyro yaw P
                0.005625f, // Gyro yaw I
                4.0f);     // Demands to rate

        hf::Level * level = new hf::Level(0.20f);

        // Add Level PID for aux switch position 1
        h.addPidController(level, 1);

        // Create DSMX Receiver object
        DSMX_Receiver * rc = new DSMX_Receiver(UARTDEV_2, CHANNEL_MAP);

        // Initialize Hackflight firmware
        h.init(new BetaFPVF3(), rc, new hf::MixerQuadXCF(), ratePid);
    }

    void loop(void)
    {
        h.update();
    }

} // extern "C"
