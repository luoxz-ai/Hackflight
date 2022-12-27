/*
   Copyright (c) 2022 Simon D. Levy

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

#pragma once

#include <stdint.h>

#include "msp.h"

class UartMsp : public Msp {

    private:

        HardwareSerial * m_port;

    protected:

        virtual void write(const uint8_t buf[], const uint8_t size) override
        {
            m_port->write(buf, size);
        }

    public:

        UartMsp(HardwareSerial & port)
        {
            m_port = &port;
        }

        virtual uint32_t available(void) override
        {
            return m_port->available();
        }

        virtual void begin(void) override
        {
            m_port->begin(115200);
        }

        virtual uint8_t read(void) override
        {
            return m_port->read();
        }

};
