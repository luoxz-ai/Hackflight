/*
   Core Hackflight class

   Copyright (c) 2021 Simon D. Levy

   MIT License
 */

#pragma once

#include "HF_sensor.hpp"
#include "HF_timer.hpp"
#include "HF_pidcontroller.hpp"
#include "HF_receiver.hpp"
#include "HF_mixer.hpp"
#include "HF_state.hpp"
#include "HF_motors.hpp"
#include "HF_debugger.hpp"

namespace hf {

    class HackflightPure {

        private:

            Timer _timer = Timer(300);

            // PID controllers
            PidController * _controllers[256] = {};
            uint8_t _controller_count = 0;

            void checkSensors(uint32_t time_usec, state_t & state)
            {
                for (uint8_t k=0; k<_sensor_count; ++k) {
                    _sensors[k]->modifyState(state, time_usec);
                }
            }

        protected:

            // Essentials
            Receiver * _receiver = NULL;
            Mixer * _mixer = NULL;
            state_t _state = {};

            // Sensors 
            Sensor * _sensors[256] = {};
            uint8_t _sensor_count = 0;

        public:

            HackflightPure(Receiver * receiver, Mixer * mixer)
            {
                _receiver = receiver;
                _mixer = mixer;
                _sensor_count = 0;
                _controller_count = 0;
            }

            void update(uint32_t time_usec, float tdmd, float rdmd, float pdmd, float ydmd, motors_t & motors)
            {
                // Start with demands from open-loop controller
                demands_t demands = {};
                _receiver->getDemands(demands);

                Debugger::printf("%+3.3f  %+3.3f\n", demands.yaw, ydmd);

                // Periodically apply PID controllers to demands
                bool ready = _timer.ready(time_usec);
                for (uint8_t k=0; k<_controller_count; ++k) {
                    _controllers[k]->modifyDemands(_state, demands, ready); 

                }

                // Use updated demands to run motors
                _mixer->run(demands, motors);

                // Check sensors
                checkSensors(time_usec, _state);
            }

            void addSensor(Sensor * sensor) 
            {
                _sensors[_sensor_count++] = sensor;
            }

            void addPidController(PidController * controller) 
            {
                _controllers[_controller_count++] = controller;
            }

    }; // class HackflightPure

} // namespace hf
