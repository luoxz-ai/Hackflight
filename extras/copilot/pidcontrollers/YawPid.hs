{--
  PID control for yaw angular velocity

  Copyright(C) 2021 Simon D.Levy

  MIT License
--}

{-# LANGUAGE RebindableSyntax #-}
{-# LANGUAGE DataKinds        #-}

module YawPid

where

import Language.Copilot
import Copilot.Compile.C99

import VehicleState
import PidController
import Demands
import Utils(constrain_abs)

yawController :: Stream Double -> Stream Double -> Stream Double -> PidController

yawController kp ki windupMax = makePidController (yawFun kp ki windupMax)


yawFun :: Stream Double -> Stream Double -> Stream Double -> PidFun

yawFun kp ki windupMax vehicleState demands =

    -- Return updated demands
    Demands 0 0 0 (kp * error' + ki * errorIntegral')

    -- Compute error as target minus actual
    where error' = (yaw demands) - (dpsi vehicleState)

          -- Accumualte error integral
          errorIntegral' = constrain_abs (errorIntegral + error') windupMax
          errorIntegral = [0] ++ errorIntegral'
