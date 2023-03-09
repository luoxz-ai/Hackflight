/*
   Copyright (c) 2023 Simon D. Levy

   This file is part of Hackflight.

   Hackflight is free software: you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation, either version 3 of the License, or (at your option)
   any later version.

   Hackflight is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
   more details.

   You should have received a copy of the GNU General Public License along with
   Hackflight. If not, see <https://www.gnu.org/licenses/>.
 */

pub struct VehicleState {

    pub x : f32,
    pub dx : f32,
    pub y : f32,
    pub dy : f32,
    pub z : f32,
    pub dz : f32,
    pub phi : f32,
    pub dphi : f32,
    pub theta : f32,
    pub dtheta : f32,
    pub psi : f32,
    pub dpsi : f32,
}

pub struct Demands {

    pub throttle: f32,
    pub roll: f32,
    pub pitch: f32,
    pub yaw: f32,
}

