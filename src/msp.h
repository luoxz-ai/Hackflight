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

class Msp {

    private:

        static const uint8_t BUF_SIZE = 128;

        typedef enum {
            IDLE,
            GOT_START,
            GOT_M,
            GOT_ARROW,
            GOT_SIZE,
            IN_PAYLOAD,
            GOT_CRC
        } parserState_t; 

        parserState_t m_parserState;

        uint8_t m_payloadChecksum;
        uint8_t m_payloadIndex;

        void serialize16(int16_t a)
        {
            serialize8(a & 0xFF);
            serialize8((a >> 8) & 0xFF);
        }

        void prepareToSerialize(uint8_t type, uint8_t count, uint8_t size)
        {
            m_payloadSize = 0;
            m_payloadIndex = 0;
            m_payloadChecksum = 0;

            addToOutBuf('$');
            addToOutBuf('M');
            addToOutBuf('>');
            serialize8(count*size);
            serialize8(type);
        }

        void addToOutBuf(uint8_t a)
        {
            m_payload[m_payloadSize++] = a;
        }

        void serialize8(uint8_t a)
        {
            addToOutBuf(a);
            m_payloadChecksum ^= a;
        }

        void prepareToSerializeBytes(uint8_t type, uint8_t count)
        {
            prepareToSerialize(type, count, 1);
        }

        void serializeByte(uint8_t src)
        {
            serialize8(src);
        }

        void prepareToSerializeInts(uint8_t type, uint8_t count)
        {
            prepareToSerialize(type, count, 4);
        }

        void prepareToSerializeFloats(uint8_t type, uint8_t count)
        {
            prepareToSerialize(type, count, 4);
        }

    protected:

        uint8_t m_payload[BUF_SIZE];
        uint8_t m_payloadSize;

    public:

        /**
          * Returns message type or 0 for not  ready
          */
        uint8_t parse(const uint8_t c)
        {
            uint8_t messageType = 0;

            static uint8_t _type;
            static uint8_t _crc;
            static uint8_t _size;
            static uint8_t _index;

            // Payload transition functions
            _size = m_parserState == GOT_ARROW ? c : _size;
            _index = m_parserState == IN_PAYLOAD ? _index + 1 : 0;
            const bool isCommand = _type >= 200;
            const bool inPayload = isCommand && m_parserState == IN_PAYLOAD;

            // Message-type transition function
            _type = m_parserState == GOT_SIZE ? c : _type;

            // Parser state transition function (final transition below)
            m_parserState
                = m_parserState == IDLE && c == '$' ? GOT_START
                : m_parserState == GOT_START && c == 'M' ? GOT_M
                : m_parserState == GOT_M && (c == '<' || c == '>') ? GOT_ARROW
                : m_parserState == GOT_ARROW ? GOT_SIZE
                : m_parserState == GOT_SIZE ? IN_PAYLOAD
                : m_parserState == IN_PAYLOAD && _index <= _size ? IN_PAYLOAD
                : m_parserState == IN_PAYLOAD ? GOT_CRC
                : m_parserState;

            // Checksum transition function
            _crc 
                = m_parserState == GOT_SIZE ?  c
                : m_parserState == IN_PAYLOAD ? _crc ^ c
                : m_parserState == GOT_CRC ? _crc 
                : 0;

            // Payload accumulation
            if (inPayload) {
                m_payload[_index-1] = c;
            }

            if (m_parserState == GOT_CRC) {

                // Message dispatch
                if (_crc == c) {
                    messageType = _type;
                }

                m_parserState = IDLE;
            }

            return messageType;

        } // parse

        bool isIdle(void)
        {
            return m_parserState == IDLE;
        }

        int16_t parseShort(uint8_t index)
        {
            int16_t s = 0;
            memcpy(&s,  &m_payload[2*index], sizeof(int16_t));
            return s;

        }

        void prepareToSerializeShorts(uint8_t messageType, uint8_t count)
        {
            prepareToSerialize(messageType, count, 2);
        }

        void completeSerialize(void)
        {
            serialize8(m_payloadChecksum);
        }

        void serializeShort(uint16_t src)
        {
            uint16_t a;
            memcpy(&a, &src, 2);
            serialize16(a);
        }

        void serializeShorts(
                const uint8_t messageType, const int16_t src[], const uint8_t count)
        {
            prepareToSerializeShorts(messageType, count);

            for (auto k=0; k<count; ++k) {
                serializeShort(src[k]);
            }

            completeSerialize();
        }

        void serializeRequest(const uint8_t messageType)
        {
            m_payload[0] = '$';
            m_payload[1] = 'M';
            m_payload[2] = '<';
            m_payload[3] = 0;
            m_payload[4] = messageType;
            m_payload[5] = messageType; // checksum (CRC)
        }

        virtual void sendPayload(void) 
        {
        }

}; // class Msp