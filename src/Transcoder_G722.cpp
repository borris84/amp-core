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
#include <cassert>

#include "Transcoder_G722.h"

namespace kc1fsz {

static const int G722_RATE = 64000;

Transcoder_G722::Transcoder_G722() {
    reset();
}

Transcoder_G722::~Transcoder_G722() {
    _destroy();
}

void Transcoder_G722::_destroy() {
    if (_enc) 
        g722_encoder_destroy(_enc);
    if (_dec) 
        g722_decoder_destroy(_dec);
    _enc = nullptr;
    _dec = nullptr;
}

void Transcoder_G722::reset() {
    // The codec is stateful (ADPCM) so a reset means a fresh context
    _destroy();
    _enc = g722_encoder_new(G722_RATE, G722_DEFAULT);
    _dec = g722_decoder_new(G722_RATE, G722_DEFAULT);
    assert(_enc != nullptr && _dec != nullptr);
}

bool Transcoder_G722::decode(const uint8_t* source, unsigned sourceLen, 
    int16_t* destPCM, unsigned destLen) {
    // Each byte decodes to two 16k samples
    if (!_dec || destLen < sourceLen * 2)
        return false;
    int n = g722_decode(_dec, source, (int)sourceLen, destPCM);
    // Pad if the decoder produced less than expected
    for (unsigned i = (n < 0 ? 0 : (unsigned)n); i < destLen; i++)
        destPCM[i] = 0;
    return n == (int)(sourceLen * 2);
}

bool Transcoder_G722::encode(const int16_t* sourcePCM, unsigned sourceLen, 
    uint8_t* dest, unsigned destLen) {
    // Two 16k samples encode to each byte
    if (!_enc || destLen < sourceLen / 2)
        return false;
    int n = g722_encode(_enc, sourcePCM, (int)sourceLen, dest);
    return n == (int)(sourceLen / 2);
}

}
