/*
   This file is part of Hackflight.

   Hackflight is free software: you can redistribute it and/or modify it under the
   terms of the GNU General Public License as published by the Free Software
   Foundation, either version 3 of the License, or (at your option) any later
   version.

   Hackflight is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
   PARTICULAR PURPOSE. See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with
   Hackflight. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdint.h>
#include <stdarg.h>

#include <vector>
using namespace std;

#include "core.h"
#include "core/mixer.h"
#include "core/motors.h"
#include "esc.h"
#include "imu.h"
#include "receiver.h"
#include "safety.h"
#include "task/accelerometer.h"
#include "task/attitude.h"
#include "task/visualizer.h"
#include "task/receiver.h"

class Board {

    private:

        uint8_t m_ledPin;
        bool m_ledInverted;

        uint32_t m_imuInterruptCount;

        VehicleState m_vstate;

        AttitudeTask m_attitudeTask = AttitudeTask(m_vstate);

        ReceiverTask m_receiverTask;

        VisualizerTask m_visualizerTask =
            VisualizerTask(m_msp, m_vstate, m_skyrangerTask);

        Msp m_msp;

        Safety m_safety;

        Core m_core;

        // Initialzed in sketch
        Esc *   m_esc;
        Mixer * m_mixer;
        vector<PidController *> * m_pidControllers;

        void startCoreTask(const uint32_t usec, float mixmotors[])
        {
            if (m_imu->gyroIsReady()) {

                auto angvels = m_imu->readGyroDps();

                m_vstate.dphi   = angvels.x;
                m_vstate.dtheta = angvels.y;
                m_vstate.dpsi   = angvels.z;
            }

            Demands demands = m_receiverTask.receiver->getDemands();

            auto motors = m_mixer->step(
                    demands,
                    m_vstate,
                    m_pidControllers,
                    m_receiverTask.receiver->gotPidReset(),
                    usec);

            for (auto i=0; i<m_mixer->getMotorCount(); i++) {

                mixmotors[i] = m_esc->getMotorValue(motors.values[i]);
            }
        }

        Safety m_saftey;

    protected:

        Board(
                Receiver & receiver,
                Imu & imu,
                vector<PidController *> & pidControllers,
                Mixer & mixer,
                Esc & esc,
                const int8_t ledPin)
        {
            m_receiverTask.receiver = &receiver;

            m_imu = &imu;
            m_pidControllers = &pidControllers;
            m_mixer = &mixer;
            m_esc = &esc;

            m_ledPin = ledPin < 0 ? -ledPin : ledPin;
            m_ledInverted = ledPin < 0;

            esc.board = this;
            receiver.board = this;
        }

    private:

       void runDynamicTasks(void)
        {
            if (m_visualizerTask.gotRebootRequest()) {
                reboot();
            }

            Task::prioritizer_t prioritizer = {Task::NONE, 0};

            const uint32_t usec = micros(); 

            m_receiverTask.prioritize(usec, prioritizer);
            m_attitudeTask.prioritize(usec, prioritizer);
            m_visualizerTask.prioritize(usec, prioritizer);

            prioritizeExtraTasks(prioritizer, usec);

            switch (prioritizer.id) {

                case Task::ATTITUDE:
                    runTask(m_attitudeTask);
                    m_safety.updateFromImu(*m_imu, m_vstate);
                    break;

                case Task::VISUALIZER:
                    runVisualizerTask();
                    break;

                case Task::RECEIVER:
                    runTask(m_receiverTask);
                    updateSafetyFromReceiver();
                   break;

                case Task::ACCELEROMETER:
                    runTask(m_accelerometerTask);
                    break;

                case Task::SKYRANGER:
                    runTask(m_skyrangerTask);
                    break;

                default:
                    break;
            }
        }

        void updateSafetyFromReceiver(void)
        {
            Safety::ledChange_e ledChange = 
                m_safety.updateFromReceiver(m_receiverTask.receiver, m_esc, micros());
            if (ledChange != Safety::LED_UNCHANGED) {
                ledSet(ledChange == Safety::LED_TURN_ON);
            }
        }

        void runTask(Task & task)
        {
            const uint32_t anticipatedEndCycles = getAnticipatedEndCycles(task);

            if (anticipatedEndCycles > 0) {

                const uint32_t usec = micros();

                task.run(usec);

                postRunTask(task, usec, anticipatedEndCycles);
            } 
        }

        void postRunTask(
                Task & task,
                const uint32_t usecStart,
                const uint32_t anticipatedEndCycles)
        {
            m_core.postRunTask(
                    task, usecStart, micros(), getCycleCounter(), anticipatedEndCycles);
        }

        void ledSet(bool on)
        {
            if (m_ledPin > 0) {
                digitalWrite(m_ledPin, m_ledInverted ? on : !on);
            }
        }

        // STM32F boards have no auto-reset bootloader support, so we reboot on
        // an external input
        virtual void reboot(void)
        {
        }

        static void outbuf(char * buf)
        {
            Serial.print(buf); 
            Serial.flush();
        }

        void runVisualizerTask(void)
        {
            const uint32_t anticipatedEndCycles =
                getAnticipatedEndCycles(m_visualizerTask);

            if (anticipatedEndCycles > 0) {

                const auto usec = micros();

                while (Serial.available()) {

                    if (m_visualizerTask.parse(Serial.read())) {
                        Serial.write(m_msp.payload, m_msp.payloadSize);
                    }
                }

                postRunTask(m_visualizerTask, usec, anticipatedEndCycles);
            }
        }

        uint32_t getAnticipatedEndCycles(Task & task)
        {
            return m_core.getAnticipatedEndCycles(task, getCycleCounter());
        }

    protected:

       // Initialized in sketch
        Imu * m_imu;

        AccelerometerTask m_accelerometerTask; 

        SkyrangerTask m_skyrangerTask = SkyrangerTask(m_vstate);

        virtual void prioritizeExtraTasks(
                Task::prioritizer_t & prioritizer, const uint32_t usec)
        {
            (void)prioritizer;
            (void)usec;
        }

    public:

        void handleImuInterrupt(void)
        {
            m_imuInterruptCount++;
            m_imu->handleInterrupt(getCycleCounter());
        }

        uint32_t microsToCycles(uint32_t micros)
        {
            return getClockSpeed() / 1000000 * micros;
        }

        virtual uint32_t getClockSpeed(void)  = 0;

        virtual uint32_t getCycleCounter(void) = 0;

        virtual void startCycleCounter(void) = 0;

        virtual void dmaInit(
                const vector<uint8_t> * motorPins, const uint32_t outputFreq)
        {
            (void)motorPins;
            (void)outputFreq;
        }

        virtual void dmaUpdateComplete(void)
        {
        }

        virtual void dmaUpdateStart(void)
        {
        }

        virtual void dmaWriteMotor(uint8_t index, uint16_t packet)
        {
            (void)index;
            (void)packet;
        }

        void begin(void)
        {
            startCycleCounter();

            m_attitudeTask.begin(m_imu);

            m_visualizerTask.begin(m_esc, m_receiverTask.receiver);

            m_imu->begin(getClockSpeed());

            m_esc->begin();

            if (m_ledPin > 0) {
                pinMode(m_ledPin, OUTPUT);
            }

            ledSet(false);
            for (auto i=0; i<Safety::STARTUP_BLINK_LED_REPS; i++) {
                static bool ledOn;
                ledOn = !ledOn;
                ledSet(ledOn);
                delay(Safety::STARTUP_BLINK_LED_DELAY);
            }
            ledSet(false);
        }

        void step(void)
        {
            auto nowCycles = getCycleCounter();

            if (m_core.isCoreTaskReady(nowCycles)) {

                const uint32_t usec = micros();

                int32_t loopRemainingCycles = 0;

                const uint32_t nextTargetCycles =
                    m_core.coreTaskPreUpdate(loopRemainingCycles);

                while (loopRemainingCycles > 0) {
                    nowCycles = getCycleCounter();
                    loopRemainingCycles = intcmp(nextTargetCycles, nowCycles);
                }
                
                float mixmotors[Motors::MAX_SUPPORTED] = {};

                startCoreTask(usec, mixmotors);

                m_esc->write(m_safety.isArmed() ?  mixmotors : m_visualizerTask.motors);

                m_core.completeCoreTask(
                        m_imu, m_imuInterruptCount, nowCycles, nextTargetCycles);
            }

            if (m_core.isDynamicTaskReady(getCycleCounter())) {
                runDynamicTasks();
            }
        }

        void step(HardwareSerial & serial)
        {
            step();

            while (m_skyrangerTask.imuDataAvailable()) {
                serial.write(m_skyrangerTask.readImuData());
            }
        }

        static void setInterrupt(
                const uint8_t pin, void (*irq)(void), const uint32_t mode)
        {
            pinMode(pin, INPUT);
            attachInterrupt(pin, irq, mode);  
        }

        static void handleReceiverSerialEvent(
                Receiver & rx, HardwareSerial & serial) {

            while (serial.available()) {

                rx.parse(serial.read(), micros());
            }
        }

        static void printf(const char * fmt, ...)
        {
            va_list ap;
            va_start(ap, fmt);
            char buf[200];
            vsnprintf(buf, 200, fmt, ap); 
            outbuf(buf);
            va_end(ap);
        }

        static void reportForever(const char * fmt, ...)
        {
            va_list ap;
            va_start(ap, fmt);
            char buf[200];
            vsnprintf(buf, 200, fmt, ap); 
            va_end(ap);

            strcat(buf, "\n");

            while (true) {
                outbuf(buf);
                delay(500);
            }
        }

}; // class Board
