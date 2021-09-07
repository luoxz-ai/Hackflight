{--
  Hackflight serial comms

  Copyright(C) 2021 on D.Levy

  MIT License
--}

{-# LANGUAGE RebindableSyntax #-}

module Serial where

import Language.Copilot

import State
import Demands

data SerialGuard = SerialGuard { available :: Stream Bool, value :: Stream Word8 }

type ParserState = Stream Word8

getSerialOut :: State -> Demands -> SerialGuard

getSerialOut _vehicleState _demands = SerialGuard false 0

  where 

    parserState = updateParser (SerialGuard serialAvailable serialByteIn) parserState'

    parserState' = [0] ++ parserState

updateParser :: SerialGuard -> ParserState -> ParserState

updateParser _serialGuard  _parserState = 0

----------------------------------------------------------

serialAvailable :: Stream Bool
serialAvailable = extern "copilot_serialAvailable" Nothing

serialByteIn :: Stream Word8
serialByteIn = extern "copilot_serialByte" Nothing
