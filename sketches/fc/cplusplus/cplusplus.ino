/*
   Hackflight example with C++ PID controllers

   Copyright (C) 2024 Simon D. Levy

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, in version 3.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http:--www.gnu.org/licenses/>.
 */

#include <turtle_board_sbus.hpp>

#include <pids/pitch_roll_angle.hpp>
#include <pids/pitch_roll_rate.hpp>
#include <pids/yaw_rate.hpp>

#include <mixers.hpp>

static hf::TurtleBoardSbus _board;

static constexpr float THROTTLE_DOWN = 0.06;

static hf::YawRatePid _yawRatePid;

static hf::PitchRollAnglePid _pitchRollAnglePid;
static hf::PitchRollRatePid _pitchRollRatePid;

void setup() 
{
    _board.init();
}

void loop() 
{
    float dt=0;
    hf::demands_t demands = {};
    hf::state_t state = {};

    _board.readData(dt, demands, state);

    const auto resetPids = demands.thrust < THROTTLE_DOWN;

    _pitchRollAnglePid.run(
            dt, resetPids, demands.roll, demands.pitch, state.phi, state.theta);

    _pitchRollRatePid.run(
            dt, resetPids, demands.roll, demands.pitch, state.dphi, state.dtheta);

    _yawRatePid.run(dt, resetPids, demands.yaw, state.dpsi);

    float m1_command=0, m2_command=0, m3_command=0, m4_command=0;

    hf::Mixer::runBetaFlightQuadX(
            demands.thrust, demands.roll, demands.pitch, demands.yaw, 
            m1_command, m2_command, m3_command, m4_command);

    _board.runMotors(m1_command, m2_command, m3_command, m4_command);
}
