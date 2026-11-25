/**
 * Copyright (C) 2025, Bruce MacKinnon KC1FSZ
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
#include <ctime>
#include <cstdint>
#include <cstring>

#include "IAX2Util.h"

namespace kc1fsz {

uint32_t makeIAX2Time() {
    /*
        The data
        field of a DATETIME information element is four octets long and
        stores the time as follows: the 5 least significant bits are seconds,
        the next 6 least significant bits are minutes, the next least
        significant 5 bits are hours, the next least significant 5 bits are
        the day of the month, the next least significant 4 bits are the
        month, and the most significant 7 bits are the year.  The year is
        offset from 2000, and the month is a 1-based index (i.e., January ==
        1, February == 2, etc.).  The timezone of the clock MUST be UTC to
        avoid confusion between the peers.    
    */
    time_t now_c_style = time(0);
    tm* t = gmtime(&now_c_style);
    // Extract individual components.  
    uint32_t result = 0;
    // Please see: https://cplusplus.com/reference/ctime/tm    
    result |= (t->tm_sec  &       0b11111);
    result |= (t->tm_min  &       0b111111) << 5;
    result |= (t->tm_hour &       0b11111) << (5 + 6);
    result |= (t->tm_mday &       0b11111) << (5 + 6 + 5);
    // tm month is 0-based and IAX2 month is 1-based
    result |= ((t->tm_mon + 1) &  0b1111) << (5 + 6 + 5 + 5);
    // tm year is offset from 1900 and IAX2 year is offset from 2000
    result |= ((t->tm_year - 100) & 0b1111111) << (5 + 6 + 5 + 5 + 4);
    return result;
}

const char* iax2TypeDesc(uint8_t type, uint8_t subclass) {
    if (type == 2) {
        if (subclass == 4) 
            return "VOICE";
    } else if (type == 4) {
        if (subclass == 4) 
            return "ANSWER";
        else if (subclass == 12) 
            return "KEY_RADIO";
        else if (subclass == 13) 
            return "UNKEY_RADIO";
        // NOT IN RFC
        else if (subclass == 255) 
            return "STOP_SOUNDS";
    } else if (type == 6) {
        if (subclass == 1) 
            return "NEW";
        else if (subclass == 2) 
            return "PING";
        else if (subclass == 3) 
            return "PONG";
        else if (subclass == 4) 
            return "ACK";
        else if (subclass == 5) 
            return "HANGUP";
        else if (subclass == 6) 
            return "REJECT";
        else if (subclass == 7) 
            return "ACCEPT";
        else if (subclass == 8) 
            return "AUTHREQ";
        else if (subclass == 9) 
            return "AUTHREP";
        else if (subclass == 0x0a) 
            return "INVAL";
        else if (subclass == 11) 
            return "LAGRQ";
        else if (subclass == 12) 
            return "LAGRP";
        else if (subclass == 0x12) 
            return "VNAK";
        else if (subclass == IAX2_SUBCLASS_IAX_POKE) 
            return "POKE";
        // NOT IN THE RFC
        else if (subclass == IAX2_SUBCLASS_IAX_CALLTOKEN) 
            return "CALLTOKEN";
    } else if (type == 7) {
        if (subclass == 0) 
            return "TEXT";
    } else if (type == 12) {
        return "DTMFPRESS";
    } else if (type == 1) {
        return "DTMF";
    } else if (type == 10) {
        return "COMFORT";
    }
    return "(UNKNOWN)";
}

int compareSeqWrap(uint8_t a, uint8_t b) {
    if (a == b)
        return 0;
    else if (a < 0x80) {
        if (b > a && b < a + 0x80)
            return -1;
        else 
            return 1;
    } else {
        if (b < a && b > a - 0x80)
            return 1;
        else
            return -1;
    }
}

/**
 * @returns The bitmask of all of the CODECs supported.
 */

uint32_t getSupportedCodecs() { 
    return CODECType::IAX2_CODEC_G711_ULAW | CODECType::IAX2_CODEC_SLIN_8K | 
        CODECType::IAX2_CODEC_SLIN_16K | CODECType::IAX2_CODEC_G726_AAL2;
}

bool isCodecSupported(CODECType type) {
    return (type & getSupportedCodecs()) != 0;
}

unsigned maxVoiceFrameSize(CODECType type) {
    if (type == CODECType::IAX2_CODEC_G726_AAL2)
        return 80;
    else if (type == CODECType::IAX2_CODEC_G711_ULAW)
        return 160;
    else if (type == CODECType::IAX2_CODEC_SLIN_8K)
        return 160 * 2;
    else if (type == CODECType::IAX2_CODEC_SLIN_16K)
        return 160 * 2 * 2;
    else 
        return 0;
}

unsigned codecSampleRate(CODECType type) {
    if (type == CODECType::IAX2_CODEC_G711_ULAW)
        return 8000;
    else if (type == CODECType::IAX2_CODEC_G726_AAL2)
        return 8000;
    else if (type == CODECType::IAX2_CODEC_SLIN_8K)
        return 8000;
    else if (type == CODECType::IAX2_CODEC_SLIN_16K)
        return 16000;
    else if (type == CODECType::IAX2_CODEC_SLIN_48K)
        return 48000;
    else if (type == CODECType::IAX2_CODEC_PCM_48K)
        return 48000;
    else 
        return 0;
}

/**
 * Fills in the array with the CODECs that are supported in preference order.
 * @returns The number of CODECs currently supported.
 */
unsigned getCodecPrefs(uint32_t* codecs, unsigned codecsCapacity) {
    unsigned count = 0;
    if (codecsCapacity > 0)
        codecs[count++] = CODECType::IAX2_CODEC_SLIN_16K;
    if (codecsCapacity > 1)
        codecs[count++] = CODECType::IAX2_CODEC_SLIN_8K;
    if (codecsCapacity > 2)
        codecs[count++] = CODECType::IAX2_CODEC_G711_ULAW;
    if (codecsCapacity > 3)
        codecs[count++] = CODECType::IAX2_CODEC_G726_AAL2;
    return count;
}

unsigned codecBlockSize(CODECType type) {
    return codecSampleRate(type) / 50;
}

/**
 * This is the 32-bit portion of Asterisk's iax2_supported_formats[] table
 * (channels/iax2/codec_pref.c). The CODEC_PREFS letter is 'A' + the 1-based
 * index in this table, NOT the bit position. Asterisk comment: "Due to a 
 * misunderstanding in how codec preferences are stored, this list starts 
 * at 'B', not 'A'".
 */
static const uint32_t prefOrder[] = {
    0x00000001, // 'B' G723
    0x00000002, // 'C' GSM
    0x00000004, // 'D' ULAW
    0x00000008, // 'E' ALAW
    0x00000800, // 'F' G726 (RFC 3551 packing, not AAL2)
    0x00000020, // 'G' ADPCM
    0x00000040, // 'H' SLIN
    0x00000080, // 'I' LPC10
    0x00000100, // 'J' G729
    0x00000200, // 'K' SPEEX
    0,          // 'L' SPEEX16 (bit 33)
    0x00000400, // 'M' ILBC
    0x00000010, // 'N' G726_AAL2
    0x00001000, // 'O' G722
    0x00008000, // 'P' SLIN16
};
static const unsigned prefOrderLen = sizeof(prefOrder) / sizeof(prefOrder[0]);

char codecMaskToLetter(uint32_t mask) {
    if (mask == 0)
        return 0;
    for (unsigned i = 0; i < prefOrderLen; i++)
        if (prefOrder[i] == mask)
            return (char)('B' + i);
    return 0;
}

uint32_t codecLetterToMask(char letter) {
    if (letter < 'B')
        return 0;
    unsigned ix = (unsigned)letter - (unsigned)'B';
    if (ix >= prefOrderLen)
        return 0;
    return prefOrder[ix];
}

unsigned makeCodecPrefString(char* buf, unsigned bufCapacity) {
    if (bufCapacity == 0)
        return 0;
    uint32_t prefs[16];
    const unsigned prefCount = getCodecPrefs(prefs, 16);
    unsigned len = 0;
    for (unsigned i = 0; i < prefCount && len + 1 < bufCapacity; i++) {
        char c = codecMaskToLetter(prefs[i]);
        if (c)
            buf[len++] = c;
    }
    buf[len] = 0;
    return len;
}

uint8_t compressCodecSubclass(uint32_t codec) {
    if (codec < 0x80)
        return (uint8_t)codec;
    for (unsigned i = 31; i >= 7; i--)
        if (codec & (1UL << i))
            return 0x80 | (uint8_t)i;
    return 0;
}

bool isLegacyAmpersandWideCodec(uint32_t narrow, uint32_t wideLow32) {
    return narrow != wideLow32;
}

unsigned parseCodecPref(const char* prefList, uint32_t* codecs, unsigned codecsCapacity,
    bool legacyAmpersand) {
    unsigned count = 0;
    while (*prefList != 0 && count < codecsCapacity) {
        uint32_t codec;
        if (legacyAmpersand) {
            // Old Ampersand mapping: 'B' + bit position
            unsigned bit = (unsigned char)*prefList - (unsigned)'B';
            codec = (*prefList >= 'B' && bit < 32) ? (1UL << bit) : 0;
        } else {
            codec = codecLetterToMask(*prefList);
        }
        if (codec)
            codecs[count++] = codec;
        prefList++;
    }
    return count;
}

uint32_t assignCodec(uint32_t callerCapability, 
    uint32_t callerDesire,
    const uint32_t* callerPrefs, unsigned callerPrefsLen,
    uint32_t calleeCapability,
    const uint32_t* calleePrefs, unsigned calleePrefsLen) {
    // First try to match the caller's desire
    if (callerDesire & calleeCapability)
        return callerDesire;
    // Next go by the caller's preferences
    for (unsigned i = 0; i < callerPrefsLen; i++)        
        if (callerPrefs[i] & callerCapability & calleeCapability)
            return callerPrefs[i];
    // If there is no match then go by the callee's preferences
    for (unsigned i = 0; i < calleePrefsLen; i++)        
        if (calleePrefs[i] & callerCapability & calleeCapability)
            return calleePrefs[i];
    return 0;
}

void fillCodecWide(uint32_t types, char* buf) {
    // Version 0 followed by the 64-bit bitfield in network byte order.
    // IMPORTANT: Asterisk lets this override the 32-bit IE.
    buf[0] = 0;
    buf[1] = buf[2] = buf[3] = buf[4] = 0;
    buf[5] = (char)((types >> 24) & 0xff);
    buf[6] = (char)((types >> 16) & 0xff);
    buf[7] = (char)((types >> 8) & 0xff);
    buf[8] = (char)(types & 0xff);
}

bool parseCodecWide(const uint8_t* buf, unsigned len, uint32_t* codecs) {
    if (len != 9 || buf[0] != 0)
        return false;
    *codecs = ((uint32_t)buf[5] << 24) | ((uint32_t)buf[6] << 16) |
        ((uint32_t)buf[7] << 8) | (uint32_t)buf[8];
    return true;
}

}
