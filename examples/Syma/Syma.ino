/*
   Hackflight sketch for Ladybug Flight Controller with Spektrum DSMX receiver

   Additional libraries needed:

       https://github.com/simondlevy/USFS
       https://github.com/simondlevy/CrossPlatformDataBus
       https://github.com/simondlevy/SpektrumDSM 

   Hardware support for Ladybug flight controller:

       https://github.com/simondlevy/grumpyoldpizza

   Copyright (C) 2021 Simon D. Levy

   MIT License

 */

#include "HF_full.hpp"
#include "hf_receivers/dsmx.hpp"
#include "hf_mixers/quad/xmw.hpp"
#include "hf_pidcontrollers/rate.hpp"
#include "hf_pidcontrollers/yaw.hpp"
#include "hf_pidcontrollers/level.hpp"
#include "hf_motors/arduino/brushed.hpp"
#include "hf_sensors/usfs.hpp"

#include <Wire.h>

static uint32_t LED_PIN = 18;

// Receiver ===================================================================

static constexpr uint8_t CHANNEL_MAP[6] = {0, 1, 2, 3, 6, 4};
static constexpr float DEMAND_SCALE = 4.0f;
static constexpr float SOFTWARE_TRIM[3] = {0, 0.05, 0.035};

static hf::DSMX_Receiver receiver = 
    hf::DSMX_Receiver(CHANNEL_MAP, DEMAND_SCALE, SOFTWARE_TRIM);  

void serialEvent1(void)
{
    while (Serial1.available()) {

        receiver.handleSerialEvent(Serial1.read(), micros());
    }
}

// Motors  =====================================================================

static hf::ArduinoBrushedMotor motor1 = hf::ArduinoBrushedMotor(13);
static hf::ArduinoBrushedMotor motor2 = hf::ArduinoBrushedMotor(A2);
static hf::ArduinoBrushedMotor motor3 = hf::ArduinoBrushedMotor(3);
static hf::ArduinoBrushedMotor motor4 = hf::ArduinoBrushedMotor(11);

// Mixer =======================================================================

static hf::MixerQuadXMW mixer = hf::MixerQuadXMW(&motor1, &motor2, &motor3, &motor4);

// PID controllers =============================================================

static hf::RatePid ratePid = hf::RatePid(0.225, 0.001875, 0.375);
static hf::YawPid yawPid = hf::YawPid(1.0625, 0.005625f);
static hf::LevelPid levelPid = hf::LevelPid(0.20f);

// Sensors =====================================================================

static hf::USFS imu;

// Serial tasks ================================================================

hf::SerialTask gcsTask;

// Hackflight object ===========================================================

static hf::HackflightFull h(&receiver, &mixer);

// Serial comms support ========================================================

namespace hf {

    bool serialAvailable(void)
    {
        return Serial.available();
    }

    uint8_t serialRead(void)
    {
        return Serial.read();
    }

    void serialWrite(uint8_t b)
    {
        Serial.write(b);
    }

} // namespace hf

// Setup =======================================================================

void setup(void)
{
    Serial.begin(115200);
    Serial1.begin(115200);
    pinMode(LED_PIN, OUTPUT);    
    Wire.begin();
    delay(100);

    // Add sensors
    h.addSensor(&imu);

    // Add PID controllers
    h.addPidController(&levelPid);
    h.addPidController(&ratePid);
    h.addPidController(&yawPid);

    // Add serial tasks
    h.addSerialTask(&gcsTask);

    // Start Hackflight firmware
    h.begin();
}

// Loop ======================================================================

void loop(void)
{
    bool led = false;

    h.update(micros(), &led);

    digitalWrite(LED_PIN, led);

}
