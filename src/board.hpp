/*

   Support for "turtle board" quadcopter

   Adapted from https://github.com/nickrehm/dRehmFlight

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

#pragma once

// Standard Arduino libraries
#include <Wire.h>
#include <SPI.h>

// Third-party libraries
#include <MPU6050.h>
#include <pmw3901.hpp>
#include <VL53L1X.h>
#include <oneshot125.hpp>
#include <dsmrx.hpp>

// Hackflight library
#include <hackflight.hpp>
#include <estimators/madgwick.hpp>
#include <msp/serializer.hpp>
#include <rx.hpp>
#include <utils.hpp>
#include <timer.hpp>

static Dsm2048 _dsm2048;

// DSMX receiver callback
void serialEvent1(void)
{
    while (Serial1.available()) {
        _dsm2048.parse(Serial1.read(), micros());
    }
}

namespace hf {

    class Board {

        // Settings we may want to adjust
        private:

            // FAFO -----------------------------------------------------------
            static const uint32_t LOOP_FREQ_HZ = 2000;

            // IMU ------------------------------------------------------------

            static const uint8_t GYRO_SCALE = MPU6050_GYRO_FS_250;
            static constexpr float GYRO_SCALE_FACTOR = 131.0;

            static const uint8_t ACCEL_SCALE = MPU6050_ACCEL_FS_2;
            static constexpr float ACCEL_SCALE_FACTOR = 16384.0;

            // LED
            static const uint8_t LED_PIN = LED_BUILTIN; // 22;
            static constexpr float BLINK_RATE_HZ = 1.5;

            // Receiver ranges for MLP6DSM transmitter
            static const uint16_t CHAN_MIN = 1158;
            static const uint16_t CHAN_MAX = 1840;

            // Throttle-down margin of safety for arming
            static constexpr uint16_t THROTTLE_SAFETY_MARGIN = 20;

            // Motors ---------------------------------------------------------
            const std::vector<uint8_t> MOTOR_PINS = { 6, 5, 4, 3 };

            // Telemetry ------------------------------------------------------
            Timer _telemetryTimer = Timer(60); // Hz

            // Debugging ------------------------------------------------------
            Timer _debugTimer = Timer(100); // Hz

            // Demand prescaling --------------------------------------------

            // Max pitch angle in degrees for angle mode (maximum ~70 degrees),
            // deg/sec for rate mode
            static constexpr float PITCH_ROLL_PRESCALE = 30.0;    

            // Max yaw rate in deg/sec
            static constexpr float YAW_PRESCALE = 160.0;     

            // IMU calibration parameters -------------------------------------

            static constexpr float ACC_ERROR_X = 0.0;
            static constexpr float ACC_ERROR_Y = 0.0;
            static constexpr float ACC_ERROR_Z = 0.0;
            static constexpr float GYRO_ERROR_X = 0.0;
            static constexpr float GYRO_ERROR_Y= 0.0;
            static constexpr float GYRO_ERROR_Z = 0.0;

        public:

            void init()
            {
                // Initialize the I^2C bus
                Wire.begin();

                // Initialize the I^2C sensors
                initImu();
                //initRangefinder();

                // Set up serial debugging
                Serial.begin(115200);

                // Set up serial connection from DSMX receiver
                Serial1.begin(115200);

                // Initialize the Madgwick filter
                _madgwick.initialize();

                // Arm OneShot125 motors
                _motors.arm();

                pinMode(LED_PIN, OUTPUT);
            }
 
            void readData(float & dt, demands_t & demands, state_t & state)
            {
                // Keep track of what time it is and how much time has elapsed
                // since the last loop
                _usec_curr = micros();      
                static uint32_t _usec_prev;
                dt = (_usec_curr - _usec_prev)/1000000.0;
                _usec_prev = _usec_curr;      

                // Read channels values from receiver
                if (_dsm2048.timedOut(micros())) {

                    _status = STATUS_FAILSAFE;
                }
                else if (_dsm2048.gotNewFrame()) {

                    _dsm2048.getChannelValues(_channels, 6);
                }

                // When throttle is down, toggle arming on switch press/release
                const auto is_arming_switch_on = _channels[5] > 1500;
                if (_channels[0] < (CHAN_MIN + THROTTLE_SAFETY_MARGIN) &&
                        !is_arming_switch_on && _was_arming_switch_on) {
                    _status = 
                        _status == STATUS_READY ? STATUS_ARMED :
                        _status == STATUS_ARMED ? STATUS_READY :
                        _status;
                }
                _was_arming_switch_on = is_arming_switch_on;

                // Read IMU
                int16_t ax=0, ay=0, az=0, gx=0, gy=0, gz=0;
                _mpu6050.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

                // Accelerometer Gs
                const axis3_t accel = {
                    -ax / ACCEL_SCALE_FACTOR - ACC_ERROR_X,
                    ay / ACCEL_SCALE_FACTOR - ACC_ERROR_Y,
                    az / ACCEL_SCALE_FACTOR - ACC_ERROR_Z
                };

                // Gyro deg /sec
                const axis3_t gyro = {
                    gx / GYRO_SCALE_FACTOR - GYRO_ERROR_X, 
                    -gy / GYRO_SCALE_FACTOR - GYRO_ERROR_Y,
                    -gz / GYRO_SCALE_FACTOR - GYRO_ERROR_Z
                };

                // Run Madgwick filter to get get Euler angles from IMU values
                // (note negations)
                axis4_t quat = {};
                _madgwick.getQuaternion(dt, gyro, accel, quat);

                // Compute Euler angles from quaternion
                axis3_t angles = {};
                Utils::quat2euler(quat, angles);

                state.phi = angles.x;
                state.theta = angles.y;
                state.psi = angles.z;

                if (false) {

                    int16_t flowDx = 0;
                    int16_t flowDy = 0;
                    bool gotFlow = false;
                    _pmw3901.readMotion(flowDx, flowDy, gotFlow); 

                    /*
                    // Read rangefinder, non-blocking
                    const uint16_t range = _vl53l1.read(false);

                    // Read optical flow sensor
                    if (gotFlow) {
                        //printf("flow: %+03d  %+03d\n", flowDx, flowDy);
                    }*/
                }

                // Get angular velocities directly from gyro
                state.dphi = gyro.x;
                state.dtheta = -gyro.y;
                state.dpsi = gyro.z;

                // Convert stick demands to appropriate intervals
                demands.thrust = mapchan(_channels[0],  0.,  1., +1);
                demands.roll   = mapchan(_channels[1], -1,  +1, -1) *
                    PITCH_ROLL_PRESCALE;
                demands.pitch  = mapchan(_channels[2], -1,  +1, +1) *
                    PITCH_ROLL_PRESCALE;
                demands.yaw    = mapchan(_channels[3], -1,  +1, -1) *
                    YAW_PRESCALE;

                // Use LED to indicate arming status
                switch (_status) {

                    case STATUS_ARMED:
                        digitalWrite(LED_PIN, HIGH);
                        break;

                    case STATUS_FAILSAFE:
                        digitalWrite(LED_PIN, LOW);
                        break;

                    default:
                        blinkLed(); // heartbeat
                        break;

                }

                // Debug periodically as needed
                if (_debugTimer.isReady(_usec_curr)) {
                }
            }

            void runMotors(const float * motors)
            {
                // Rescale motor values for OneShot125
                auto m1_usec = scaleMotor(motors[0]);
                auto m2_usec = scaleMotor(motors[1]);
                auto m3_usec = scaleMotor(motors[2]);
                auto m4_usec = scaleMotor(motors[3]);

                // Turn off motors under various conditions
                if (_channels[4] < 1500) {

                    if (_status == STATUS_ARMED) {
                        _status = STATUS_READY;
                    }
                    m1_usec = 120;
                    m2_usec = 120;
                    m3_usec = 120;
                    m4_usec = 120;
                }

                // Run motors
                _motors.set(0, m1_usec);
                _motors.set(1, m2_usec);
                _motors.set(2, m3_usec);
                _motors.set(3, m4_usec);

                _motors.run();

                runLoopDelay(_usec_curr);
            }

        private:

            // Sensors
            MPU6050 _mpu6050;
            VL53L1X _vl53l1;
            PMW3901 _pmw3901;

            // Motors
            OneShot125 _motors = OneShot125(MOTOR_PINS);

            // Timing
            uint32_t _usec_curr;

            // Receiver
            uint16_t _channels[6];

            // Safety
            uint8_t _status;
            bool _was_arming_switch_on;

            // State estimation
            MadgwickFilter  _madgwick;

            // Private methods -----------------------------------------------

            static float mapchan(
                    const uint16_t rawval,
                    const float newmin,
                    const float newmax,
                    const float sign)
            {
                return sign * (newmin + (rawval - (float)CHAN_MIN) /
                    (CHAN_MAX - CHAN_MIN) * (newmax - newmin));
            }

            static void runLoopDelay(const uint32_t usec_curr)
            {
                //DESCRIPTION: Regulate main loop rate to specified frequency
                //in Hz
                /*
                 * It's good to operate at a constant loop rate for filters to
                 * remain stable and whatnot. Interrupt routines running in the
                 * background cause the loop rate to fluctuate. This function
                 * basically just waits at the end of every loop iteration
                 * until the correct time has passed since the start of the
                 * current loop for the desired loop rate in Hz. 2kHz is a good
                 * rate to be at because the loop nominally will run between
                 * 2.8kHz - 4.2kHz. This lets us have a little room to add
                 * extra computations and remain above 2kHz, without needing to
                 * retune all of our filtering parameters.
                 */
                float invFreq = 1.0/LOOP_FREQ_HZ*1000000.0;

                unsigned long checker = micros();

                // Sit in loop until appropriate time has passed
                while (invFreq > (checker - usec_curr)) {
                    checker = micros();
                }
            }

            void initImu() 
            {
                //Note this is 2.5 times the spec sheet 400 kHz max...
                Wire.setClock(1000000); 

                _mpu6050.initialize();

                if (!_mpu6050.testConnection()) {
                    reportForever("MPU6050 initialization unsuccessful\n");
                }

                // From the reset state all registers should be 0x00, so we
                // should be at max sample rate with digital low pass filter(s)
                // off.  All we need to do is set the desired fullscale ranges
                _mpu6050.setFullScaleGyroRange(GYRO_SCALE);
                _mpu6050.setFullScaleAccelRange(ACCEL_SCALE);
            }

            void blinkLed()
            {
                static uint32_t _usec_prev;

                static uint32_t _delay_usec;

                if (_usec_curr - _usec_prev > _delay_usec) {

                    static bool _alternate;

                    _usec_prev = _usec_curr;

                    digitalWrite(LED_PIN, _alternate);

                    if (_alternate) {
                        _alternate = false;
                        _delay_usec = 100'100;
                    }
                    else {
                        _alternate = true;
                        _delay_usec = BLINK_RATE_HZ * 1e6;
                    }
                }
            }

            void initRangefinder()
            {
                _vl53l1.setTimeout(500);

                if (!_vl53l1.init()) {
                    reportForever("VL53L1 initialization unsuccessful");
                }

                // Use long distance mode and allow up to 50000 us (50 ms) for
                // a measurement.  You can change these settings to adjust the
                // performance of the _vl53l1, but the minimum timing budget is
                // 20 ms for short distance mode and 33 ms for medium and long
                // distance modes. See the VL53L1X datasheet for more
                // information on range and timing limits.
                _vl53l1.setDistanceMode(VL53L1X::Long);
                _vl53l1.setMeasurementTimingBudget(50000);

                // Start continuous readings at a rate of one measurement every
                // 50 ms (the inter-measurement period). This period should be
                // at least as long as the timing budget.
                _vl53l1.startContinuous(50);
            }

            void initOpticalFlow()
            {
                if (!_pmw3901.begin()) {
                    reportForever("PMW3901 initialization unsuccessful");
                }
            }

            static uint8_t scaleMotor(const float mval)
            {
                return Utils::u8constrain(mval*125 + 125, 125, 250);

            }

            static void reportForever(const char * message)
            {
                while (true) {
                    printf("%s\n", message);
                    delay(500);
                }

            }
    };

}
