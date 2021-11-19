{--
  Motor configurations and safety features

  Copyright(C) 2021 Simon D.Levy

  MIT License
--}

{-# LANGUAGE RebindableSyntax #-}

module Motors where

import Language.Copilot

import Utils

data Motors = Quad { m1 :: SFloat
                   , m2 :: SFloat
                   , m3 :: SFloat
                   , m4 :: SFloat }

data Spins = Spins { s1 :: SFloat
                   , s2 :: SFloat
                   , s3 :: SFloat
                   , s4 :: SFloat }

type MotorFun = Motors -> SBool -> SWord8 -> SWord8 -> Motors

motorval :: SBool -> SFloat -> SWord8 -> SWord8 -> SWord8  -> SFloat

motorval armed flyval index target percent =
  if armed then flyval
  else if index == target then (unsafeCast percent) / 100
  else 0

quadfun :: MotorFun

quadfun motors armed index percent = Quad m1val m2val m3val m4val where

  m1val = motorval armed (m1 motors) index 1 percent
  m2val = motorval armed (m2 motors) index 2 percent
  m3val = motorval armed (m3 motors) index 3 percent
  m4val = motorval armed (m4 motors) index 4 percent

motors' :: Motors -> Motors

motors' (Quad m1 m2 m3 m4) = Quad ([0] ++ m1) ([0] ++ m2) ([0] ++ m3) ([0] ++ m4)
