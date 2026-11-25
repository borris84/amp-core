/**
 * Copyright (C) 2026, Ampersand project contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <cstdint>

#include "amp/Ampersand.h"
#include "Transcoder.h"

// From libg722 (https://github.com/sippy/libg722)
#include "g722_encoder.h"
#include "g722_decoder.h"

namespace kc1fsz {

/**
 * G.722 at 64 kbit/s with a 16 kHz PCM side. One 20ms block is 
 * 320 PCM samples <-> 160 bytes of G.722. This matches Asterisk's 
 * codec_g722 (slin16 <-> g722, 64000 bps, no packing needed at 64k).
 */
class Transcoder_G722 : public Transcoder {
public:

    Transcoder_G722();
    ~Transcoder_G722();
    Transcoder_G722(const Transcoder_G722&) = delete;
    Transcoder_G722& operator=(const Transcoder_G722&) = delete;

    virtual bool decode(const uint8_t* source, unsigned sourceLen, 
        int16_t* dest, unsigned destLen);
    virtual bool encode(const int16_t* source, unsigned sourceLen, 
        uint8_t* dest, unsigned destLen);
    virtual void reset();

private:

    void _destroy();

    G722_ENC_CTX* _enc = nullptr;
    G722_DEC_CTX* _dec = nullptr;
};

}
