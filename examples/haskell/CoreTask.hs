{--
  LambdaFlight core algorithm: reads open-loop demands and
  state as streams; runs PID controllers and motor mixers
 
  Copyright (C) 2024 Simon D. Levy
 
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, in version 3.
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
--} 

{-# LANGUAGE DataKinds #-}
{-# LANGUAGE RebindableSyntax #-}

module CoreTask where

import Language.Copilot
import Copilot.Compile.C99

import Demands
import Mixers
import Utils

-- Streams from C++ ----------------------------------------------------------

thro_demand :: SFloat
thro_demand = extern "stream_thro_demand" Nothing

roll_PID :: SFloat
roll_PID = extern "stream_roll_PID" Nothing

pitch_PID :: SFloat
pitch_PID = extern "stream_pitch_PID" Nothing

yaw_PID :: SFloat
yaw_PID = extern "stream_yaw_PID" Nothing

------------------------------------------------------------------------------

step :: (SFloat, SFloat, SFloat, SFloat)

step = motors where

  motors = runBetaFlightQuadX $ Demands thro_demand roll_PID pitch_PID yaw_PID
------------------------------------------------------------------------------
 
spec = do

    let motors = step

    let (m1, m2, m3, m4) = motors

    trigger "setMotors" true [arg $ m1, arg $ m2, arg $ m3, arg $ m4] 

-- Compile the spec
main = reify spec >>= 
  compileWith (CSettings "copilot_step_core" ".") "copilot_core"
