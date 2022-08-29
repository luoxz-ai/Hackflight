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

#include <stdint.h>
#include <math.h>
#include <string.h>

#include <esc.h>

#include "platform.h"
#include "io.h"
#include "io_impl.h"
#include "dma.h"
#include "dma_reqmap.h"
#include "dshot.h"
#include "dshot_bitbang.h"
#include "dshot_bitbang_impl.h"
#include "dshot_command.h"
#include "motordev.h"
#include "nvic.h"
#include "pwm_output.h" 
#include "dshot_dpwm.h"
#include "dshot_bitbang_decode.h"
#include "systemdev.h"
#include <time.h>
#include "timer.h"

typedef enum {
    DSHOT_BITBANGED_TIMER_AUTO = 0,
    DSHOT_BITBANGED_TIMER_TIM1,
    DSHOT_BITBANGED_TIMER_TIM8,
} dshotBitbangedTimer_e;


static uint8_t USE_BITBANGED_TIMER = 0;

FAST_DATA_ZERO_INIT bbPacer_t bbPacers[MAX_MOTOR_PACERS];  // TIM1 or TIM8
FAST_DATA_ZERO_INIT int usedMotorPacers = 0;

FAST_DATA_ZERO_INIT bbPort_t bbPorts[MAX_SUPPORTED_MOTOR_PORTS];
FAST_DATA_ZERO_INIT int usedMotorPorts;

FAST_DATA_ZERO_INIT bbMotor_t bbMotors[MAX_SUPPORTED_MOTORS];

static FAST_DATA_ZERO_INIT int motorCount;
dshotBitbangStatus_e bbStatus;

// For MCUs that use MPU to control DMA coherency, there might be a performance hit
// on manipulating input buffer content especially if it is read multiple times,
// as the buffer region is attributed as not cachable.
// If this is not desirable, we should use manual cache invalidation.
#define BB_OUTPUT_BUFFER_ATTRIBUTE
#define BB_INPUT_BUFFER_ATTRIBUTE

BB_OUTPUT_BUFFER_ATTRIBUTE uint32_t
  bbOutputBuffer[MOTOR_DSHOT_BUF_CACHE_ALIGN_LENGTH * MAX_SUPPORTED_MOTOR_PORTS];
BB_INPUT_BUFFER_ATTRIBUTE uint16_t
  bbInputBuffer[DSHOT_BB_PORT_IP_BUF_CACHE_ALIGN_LENGTH * MAX_SUPPORTED_MOTOR_PORTS];

uint8_t bbPuPdMode;
FAST_DATA_ZERO_INIT uint32_t dshotFrameUs;


const timerHardware_t bbTimerHardware[] = {
    DEF_TIM(TIM1,  CH1, NONE,  TIM_USE_NONE, 0, 1),
    DEF_TIM(TIM1,  CH1, NONE,  TIM_USE_NONE, 0, 2),
    DEF_TIM(TIM1,  CH2, NONE,  TIM_USE_NONE, 0, 1),
    DEF_TIM(TIM1,  CH3, NONE,  TIM_USE_NONE, 0, 1),
    DEF_TIM(TIM1,  CH4, NONE,  TIM_USE_NONE, 0, 0),
};

static FAST_DATA_ZERO_INIT motorDevice_t bbDevice;
static FAST_DATA_ZERO_INIT uint32_t lastSendUs;


// DMA GPIO output buffer formatting

static void bbOutputDataInit(uint32_t *buffer, uint16_t portMask, bool inverted)
{
    uint32_t resetMask;
    uint32_t setMask;

    if (inverted) {
        resetMask = portMask;
        setMask = (portMask << 16);
    } else {
        resetMask = (portMask << 16);
        setMask = portMask;
    }

    int bitpos;

    for (bitpos = 0; bitpos < 16; bitpos++) {
        buffer[bitpos * 3 + 0] |= setMask ; // Always set all ports
        buffer[bitpos * 3 + 1] = 0;          // Reset bits are port dependent
        buffer[bitpos * 3 + 2] |= resetMask; // Always reset all ports
    }
}

static void bbOutputDataSet(uint32_t *buffer, int pinNumber, uint16_t value, bool inverted)
{
    uint32_t middleBit;

    if (inverted) {
        middleBit = (1 << (pinNumber + 0));
    } else {
        middleBit = (1 << (pinNumber + 16));
    }

    for (int pos = 0; pos < 16; pos++) {
        if (!(value & 0x8000)) {
            buffer[pos * 3 + 1] |= middleBit;
        }
        value <<= 1;
    }
}

static void bbOutputDataClear(uint32_t *buffer)
{
    // Middle position to no change
    for (int bitpos = 0; bitpos < 16; bitpos++) {
        buffer[bitpos * 3 + 1] = 0;
    }
}

// bbPacer management

static bbPacer_t *bbFindMotorPacer(TIM_TypeDef *tim)
{
    for (int i = 0; i < MAX_MOTOR_PACERS; i++) {

        bbPacer_t *bbPacer = &bbPacers[i];

        if (bbPacer->tim == NULL) {
            bbPacer->tim = tim;
            ++usedMotorPacers;
            return bbPacer;
        }

        if (bbPacer->tim == tim) {
            return bbPacer;
        }
    }

    return NULL;
}

// bbPort management

static bbPort_t *bbFindMotorPort(int portIndex)
{
    for (int i = 0; i < usedMotorPorts; i++) {
        if (bbPorts[i].portIndex == portIndex) {
            return &bbPorts[i];
        }
    }
    return NULL;
}

static bbPort_t *bbAllocateMotorPort(int portIndex)
{
    if (usedMotorPorts >= MAX_SUPPORTED_MOTOR_PORTS) {
        bbStatus = DSHOT_BITBANG_STATUS_TOO_MANY_PORTS;
        return NULL;
    }

    bbPort_t *bbPort = &bbPorts[usedMotorPorts];

    if (!bbPort->timhw) {
        // No more pacer channel available
        bbStatus = DSHOT_BITBANG_STATUS_NO_PACER;
        return NULL;
    }

    bbPort->portIndex = portIndex;
    bbPort->owner.owner = OWNER_DSHOT_BITBANG;
    bbPort->owner.resourceIndex = RESOURCE_INDEX(portIndex);

    ++usedMotorPorts;

    return bbPort;
}

const timerHardware_t *dshotBitbangTimerGetAllocatedByNumberAndChannel(
        int8_t timerNumber,
        uint16_t timerChannel)
{
    for (int index = 0; index < usedMotorPorts; index++) {
        const timerHardware_t *bitbangTimer = bbPorts[index].timhw;
        if (bitbangTimer && timerGetTIMNumber(bitbangTimer->tim) == timerNumber
                && bitbangTimer->channel == timerChannel &&
                bbPorts[index].owner.owner) { return bitbangTimer;
        }
    }

    return NULL;
}

const resourceOwner_t *dshotBitbangTimerGetOwner(const timerHardware_t *timer)
{
    for (int index = 0; index < usedMotorPorts; index++) {
        const timerHardware_t *bitbangTimer = bbPorts[index].timhw;
        if (bitbangTimer && bitbangTimer == timer) {
            return &bbPorts[index].owner;
        }
    }

    return &freeOwner;
}

// Return frequency of smallest change [state/sec]

static uint32_t getDshotBaseFrequency(g_motorPwmProtocolTypes_e pwmProtocolType)
{
    switch (pwmProtocolType) {
        case(PWM_TYPE_DSHOT600):
            return MOTOR_DSHOT600_SYMBOL_RATE * MOTOR_DSHOT_STATE_PER_SYMBOL;
        case(PWM_TYPE_DSHOT300):
            return MOTOR_DSHOT300_SYMBOL_RATE * MOTOR_DSHOT_STATE_PER_SYMBOL;
        default:
        case(PWM_TYPE_DSHOT150):
            return MOTOR_DSHOT150_SYMBOL_RATE * MOTOR_DSHOT_STATE_PER_SYMBOL;
    }
}

static void bbSetupDma(bbPort_t *bbPort)
{
    const dmaIdentifier_e dmaIdentifier = dmaGetIdentifier(bbPort->dmaResource);
    dmaEnable(dmaIdentifier);
    bbPort->dmaSource = timerDmaSource(bbPort->timhw->channel);

    bbPacer_t *bbPacer = bbFindMotorPacer(bbPort->timhw->tim);
    bbPacer->dmaSources |= bbPort->dmaSource;

    dmaSetHandler(dmaIdentifier, bbDMAIrqHandler, NVIC_BUILD_PRIORITY(2, 1), (uint32_t)bbPort);

    bbDMA_ITConfig(bbPort);
}

void bbDMAIrqHandler(dmaChannelDescriptor_t *descriptor)
{
    bbPort_t *bbPort = (bbPort_t *)descriptor->userParam;

    bbDMA_Cmd(bbPort, DISABLE);

    bbTIM_DMACmd(bbPort->timhw->tim, bbPort->dmaSource, DISABLE);

    if (DMA_GET_FLAG_STATUS(descriptor, DMA_IT_TEIF)) {
        while (1) {};
    }

    DMA_CLEAR_FLAG(descriptor, DMA_IT_TCIF);
}

// Setup bbPorts array elements so that they each have a TIM1 or TIM8 channel
// in timerHardware array for BB-DShot.

static void bbFindPacerTimer(void)
{
    for (int bbPortIndex = 0; bbPortIndex < MAX_SUPPORTED_MOTOR_PORTS; bbPortIndex++) {
        for (unsigned timerIndex = 0; timerIndex < ARRAYLEN(bbTimerHardware); timerIndex++) {
            const timerHardware_t *timer = &bbTimerHardware[timerIndex];
            int timNumber = timerGetTIMNumber(timer->tim);
            if ((USE_BITBANGED_TIMER == DSHOT_BITBANGED_TIMER_TIM1 && timNumber != 1)
                    || (USE_BITBANGED_TIMER == DSHOT_BITBANGED_TIMER_TIM8 && timNumber != 8)) {
                continue;
            }
            bool timerConflict = false;
            for (int channel = 0; channel < CC_CHANNELS_PER_TIMER; channel++) {
                const timerHardware_t *timer =
                    timerGetAllocatedByNumberAndChannel(timNumber,
                            CC_CHANNEL_FROM_INDEX(channel)); const
                    resourceOwner_e timerOwner = timerGetOwner(timer)->owner;
                if (timerOwner != OWNER_FREE && timerOwner != OWNER_DSHOT_BITBANG) {
                    timerConflict = true;
                    break;
                }
            }

            for (int index = 0; index < bbPortIndex; index++) {
                const timerHardware_t* t = bbPorts[index].timhw;
                if (timerGetTIMNumber(t->tim) == timNumber && timer->channel == t->channel) {
                    timerConflict = true;
                    break;
                }
            }

            if (timerConflict) {
                continue;
            }

            dmaoptValue_t dmaopt = dmaGetOptionByTimer(timer);
            const dmaChannelSpec_t *dmaChannelSpec =
                dmaGetChannelSpecByTimerValue(timer->tim, timer->channel,
                        dmaopt); dmaResource_t *dma = dmaChannelSpec->ref;
            dmaIdentifier_e dmaIdentifier = dmaGetIdentifier(dma);
            if (dmaGetOwner(dmaIdentifier)->owner == OWNER_FREE) {
                bbPorts[bbPortIndex].timhw = timer;

                break;
            }
        }
    }
}

static void bbTimebaseSetup(bbPort_t *bbPort, g_motorPwmProtocolTypes_e dshotProtocolType)
{
    uint32_t timerclock = timerClock(bbPort->timhw->tim);

    uint32_t outputFreq = getDshotBaseFrequency(dshotProtocolType);
    dshotFrameUs = 1000000 * 17 * 3 / outputFreq;
    bbPort->outputARR = timerclock / outputFreq - 1;

    // XXX Explain this formula
    uint32_t inputFreq = outputFreq * 5 * 2 * DSHOT_BITBANG_TELEMETRY_OVER_SAMPLE / 24;
    bbPort->inputARR = timerclock / inputFreq - 1;
}

//
// bb only use pin info associated with timerHardware entry designated as TIM_USE_MOTOR;
// it does not use the timer channel associated with the pin.
//

static bool bbMotorConfig(IO_t io, uint8_t motorIndex, g_motorPwmProtocolTypes_e pwmProtocolType, uint8_t output)
{
    int pinIndex = IO_GPIOPinIdx(io);
    int portIndex = IO_GPIOPortIdx(io);

    bbPort_t *bbPort = bbFindMotorPort(portIndex);

    if (!bbPort) {

        // New port group

        bbPort = bbAllocateMotorPort(portIndex);

        if (bbPort) {
            const timerHardware_t *timhw = bbPort->timhw;
            const dmaChannelSpec_t *dmaChannelSpec =
                dmaGetChannelSpecByTimerValue(timhw->tim, timhw->channel,
                        dmaGetOptionByTimer(timhw)); bbPort->dmaResource =
                dmaChannelSpec->ref;
            bbPort->dmaChannel = dmaChannelSpec->channel;
        }

        if (!bbPort || !dmaAllocate(dmaGetIdentifier(bbPort->dmaResource),
                    bbPort->owner.owner, bbPort->owner.resourceIndex)) {
            bbDevice.vTable.write = motorDevWriteNull;
            bbDevice.vTable.updateStart = motorUpdateStartNull;
            bbDevice.vTable.updateComplete = motorUpdateCompleteNull;

            return false;
        }

        bbPort->gpio = IO_GPIO(io);

        bbPort->portOutputCount = MOTOR_DSHOT_BUF_LENGTH;
        bbPort->portOutputBuffer = &bbOutputBuffer[(bbPort - bbPorts) *
            MOTOR_DSHOT_BUF_CACHE_ALIGN_LENGTH];

        bbPort->portInputCount = DSHOT_BB_PORT_IP_BUF_LENGTH;
        bbPort->portInputBuffer = &bbInputBuffer[(bbPort - bbPorts) *
            DSHOT_BB_PORT_IP_BUF_CACHE_ALIGN_LENGTH];

        bbTimebaseSetup(bbPort, pwmProtocolType);
        bbTIM_TimeBaseInit(bbPort, bbPort->outputARR);
        bbTimerChannelInit(bbPort);

        bbSetupDma(bbPort);
        bbDMAPreconfigure(bbPort, DSHOT_BITBANG_DIRECTION_OUTPUT);
        bbDMAPreconfigure(bbPort, DSHOT_BITBANG_DIRECTION_INPUT);

        bbDMA_ITConfig(bbPort);
    }

    bbMotors[motorIndex].pinIndex = pinIndex;
    bbMotors[motorIndex].io = io;
    bbMotors[motorIndex].output = output;
    bbMotors[motorIndex].bbPort = bbPort;

    IOInit(io, OWNER_MOTOR, RESOURCE_INDEX(motorIndex));

    // Setup GPIO_MODER and GPIO_ODR register manipulation values

    bbGpioSetup(&bbMotors[motorIndex]);

    bbOutputDataInit(bbPort->portOutputBuffer, (1 << pinIndex), DSHOT_BITBANG_NONINVERTED);

    bbSwitchToOutput(bbPort);

    bbMotors[motorIndex].configured = true;

    return true;
}

static bool bbUpdateStart(void)
{
    for (int i = 0; i < usedMotorPorts; i++) {
        bbDMA_Cmd(&bbPorts[i], DISABLE);
        bbOutputDataClear(bbPorts[i].portOutputBuffer);
    }

    return true;
}

static void bbWriteInt(uint8_t motorIndex, uint16_t value)
{
    bbMotor_t *const bbmotor = &bbMotors[motorIndex];

    if (!bbmotor->configured) {
        return;
    }

    // fetch requestTelemetry from motors. Needs to be refactored.
    motorDmaOutput_t * const motor = getMotorDmaOutput(motorIndex);
    bbmotor->protocolControl.requestTelemetry = motor->protocolControl.requestTelemetry;
    motor->protocolControl.requestTelemetry = false;

    // If there is a command ready to go overwrite the value and send that instead
    if (dshotCommandIsProcessing()) {
        value = dshotCommandGetCurrent(motorIndex);
        if (value) {
            bbmotor->protocolControl.requestTelemetry = true;
        }
    }

    bbmotor->protocolControl.value = value;

    uint16_t packet = prepareDshotPacket(&bbmotor->protocolControl);

    bbPort_t *bbPort = bbmotor->bbPort;

    bbOutputDataSet(bbPort->portOutputBuffer, bbmotor->pinIndex, packet, DSHOT_BITBANG_NONINVERTED);
}

static void bbWrite(uint8_t motorIndex, float value)
{
    bbWriteInt(motorIndex, value);
}

static void bbUpdateComplete(void)
{
    // If there is a dshot command loaded up, time it correctly with motor update

    if (!dshotCommandQueueEmpty()) {
        if (!dshotCommandOutputIsEnabled(bbDevice.count)) {
            return;
        }
    }

    for (int i = 0; i < usedMotorPorts; i++) {
        bbPort_t *bbPort = &bbPorts[i];

        bbDMA_Cmd(bbPort, ENABLE);
    }

    lastSendUs = micros();
    for (int i = 0; i < usedMotorPacers; i++) {
        bbPacer_t *bbPacer = &bbPacers[i];
        bbTIM_DMACmd(bbPacer->tim, bbPacer->dmaSources, ENABLE);
    }
}

static bool bbEnableMotors(void)
{
    for (int i = 0; i < motorCount; i++) {
        if (bbMotors[i].configured) {
            IOConfigGPIO(bbMotors[i].io, bbMotors[i].iocfg);
        }
    }
    return true;
}

static void bbDisableMotors(void)
{
    return;
}

static void bbShutdown(void)
{
    return;
}

static bool bbIsMotorEnabled(uint8_t index)
{
    return bbMotors[index].enabled;
}

static void bbPostInit()
{
    bbFindPacerTimer();

    for (int motorIndex = 0; motorIndex < MAX_SUPPORTED_MOTORS && motorIndex <
            motorCount; motorIndex++) {

        if (!bbMotorConfig(bbMotors[motorIndex].io, motorIndex,
                    MOTOR_PWM_PROTOCOL, bbMotors[motorIndex].output)) { return
            NULL;
        }


        bbMotors[motorIndex].enabled = true;

        // Fill in motors structure for 4way access (XXX Should be refactored)

        motors[motorIndex].enabled = true;
    }
}

static motorVTable_t bbVTable = {
    .postInit = bbPostInit,
    .enable = bbEnableMotors,
    .disable = bbDisableMotors,
    .isMotorEnabled = bbIsMotorEnabled,
    .updateStart = bbUpdateStart,
    .write = bbWrite,
    .writeInt = bbWriteInt,
    .updateComplete = bbUpdateComplete,
    .convertExternalToMotor = dshotConvertFromExternal,
    .convertMotorToExternal = dshotConvertToExternal,
    .shutdown = bbShutdown,
};

dshotBitbangStatus_e dshotBitbangGetStatus()
{
    return bbStatus;
}

motorDevice_t *dshotBitbangDevInit(uint8_t count)
{
    bbDevice.vTable = bbVTable;
    motorCount = count;
    bbStatus = DSHOT_BITBANG_STATUS_OK;

    memset(bbOutputBuffer, 0, sizeof(bbOutputBuffer));

    for (int motorIndex = 0; motorIndex < MAX_SUPPORTED_MOTORS && motorIndex < motorCount; motorIndex++) {
        const timerHardware_t *timerHardware = timerGetConfiguredByTag(MOTOR_IO_TAGS[motorIndex]);
        const IO_t io = IOGetByTag(MOTOR_IO_TAGS[motorIndex]);

        uint8_t output = timerHardware->output;
        bbPuPdMode = (output & TIMER_OUTPUT_INVERTED) ? BB_GPIO_PULLDOWN : BB_GPIO_PULLUP;

        if (!IOIsFreeOrPreinit(io)) {
            /* not enough motors initialised for the mixer or a break in the motors */
            bbDevice.vTable.write = motorDevWriteNull;
            bbDevice.vTable.updateStart = motorUpdateStartNull;
            bbDevice.vTable.updateComplete = motorUpdateCompleteNull;
            bbStatus = DSHOT_BITBANG_STATUS_MOTOR_PIN_CONFLICT;
            return NULL;
        }

        int pinIndex = IO_GPIOPinIdx(io);

        bbMotors[motorIndex].pinIndex = pinIndex;
        bbMotors[motorIndex].io = io;
        bbMotors[motorIndex].output = output;
        bbMotors[motorIndex].iocfg = IO_CONFIG(GPIO_Mode_OUT, GPIO_Speed_50MHz,
                GPIO_OType_PP, bbPuPdMode);

        IOInit(io, OWNER_MOTOR, RESOURCE_INDEX(motorIndex));
        IOConfigGPIO(io, bbMotors[motorIndex].iocfg);
        if (output & TIMER_OUTPUT_INVERTED) {
            IOLo(io);
        } else {
            IOHi(io);
        }

        // Fill in motors structure for 4way access (XXX Should be refactored)
        motors[motorIndex].io = bbMotors[motorIndex].io;
    }

    return &bbDevice;
}
