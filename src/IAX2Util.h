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
#pragma once

#include <cstdint>

namespace kc1fsz {

/**
 * Creates a date/time following the standard in RFC 5456 
 * Section 8.6.28.
 * 
 * See: https://datatracker.ietf.org/doc/html/rfc5456#section-8.6.28
 */
uint32_t makeIAX2Time();

/**
 * Helpful for debug.
 */
const char* iax2TypeDesc(uint8_t type, uint8_t subclass);

enum FrameType {
    IAX2_TYPE_DTMF = 0x01,
    IAX2_TYPE_VOICE = 0x02,
    IAX2_TYPE_CONTROL = 0x04,
    IAX2_TYPE_IAX = 0x06,
    IAX2_TYPE_TEXT = 0x07,
    // NOT IN RFC
    IAX2_TYPE_DTMF2 = 0x0c
};

enum IAXSubclass {
    IAX2_SUBCLASS_IAX_NEW = 0x01,
    IAX2_SUBCLASS_IAX_PING = 0x02,
    IAX2_SUBCLASS_IAX_PONG = 0x03,
    IAX2_SUBCLASS_IAX_ACK = 0x04,
    IAX2_SUBCLASS_IAX_HANGUP = 0x05,
    IAX2_SUBCLASS_IAX_REJECT = 0x06,
    IAX2_SUBCLASS_IAX_ACCEPT = 0x07,
    IAX2_SUBCLASS_IAX_AUTHREQ = 0x08,
    IAX2_SUBCLASS_IAX_AUTHREP = 0x09,
    IAX2_SUBCLASS_IAX_INVAL = 0x0a,
    IAX2_SUBCLASS_IAX_LAGRQ = 0x0b,
    IAX2_SUBCLASS_IAX_LAGRP = 0x0c,
    IAX2_SUBCLASS_IAX_REGREQ = 0x0d,
    IAX2_SUBCLASS_IAX_REGAUTH = 0x0e,
    IAX2_SUBCLASS_IAX_REGACK = 0x0f,
    IAX2_SUBCLASS_IAX_VNAK = 0x12,
    IAX2_SUBCLASS_IAX_POKE = 0x1e,
    // NOT IN RFC
    IAX2_SUBCLASS_IAX_CALLTOKEN = 40
};

enum ControlSubclass {
    IAX2_SUBCLASS_CONTROL_ANSWER = 0x04,
    IAX2_SUBCLASS_CONTROL_UNKEY = 0x0d,
    // NOT IN RFC
    IAX2_SUBCLASS_CONTROL_STOP_SOUNDS = 255
};

// See: https://datatracker.ietf.org/doc/html/rfc5457#page-19
// These bit positions match Asterisk's AST_FORMAT_xxx bitfield
// (include/asterisk/format_compatibility.h).
enum CODECType {
    IAX2_CODEC_UNKNOWN    = 0,
    IAX2_CODEC_GSM_FULL   = 0x00000002,
    IAX2_CODEC_G711_ULAW  = 0x00000004,
    IAX2_CODEC_G711_ALAW  = 0x00000008,
    // 8k
    IAX2_CODEC_G726_AAL2  = 0x00000010,
    // (8k 16-bit SLIN, little endian)
    IAX2_CODEC_SLIN_8K    = 0x00000040,
    // 16k G.722 at 64 kbit/s (one byte per pair of 16k samples)
    IAX2_CODEC_G722       = 0x00001000,
    // (16k 16-bit SLIN, little endian)
    IAX2_CODEC_SLIN_16K   = 0x00008000,
    // (48k 16-bit SLIN, little endian)
    // NOT OFFICIAL!
    IAX2_CODEC_SLIN_48K   = 0x20000000,
    // 16-bit PCM audio in native format
    // NOT OFFICIAL!
    IAX2_CODEC_PCM_48K    = 0x40000000
};

/**
 * @returns True if the specified CODEC is supported
 */
bool isCodecSupported(CODECType type);

/**
 * @returns The bitmask of all of the CODECs supported.
 */
uint32_t getSupportedCodecs();

/**
 * Enables/disables an optional CODEC process-wide. At the moment the
 * only optional CODEC is G.722, which is disabled by default.
 */
void setCodecEnabled(CODECType type, bool enabled);

/**
 * Fills in the array with the CODECs that are supported in preference order.
 * @returns The number of CODECs currently supported.
 */
unsigned getCodecPrefs(uint32_t* codecs, unsigned codecsCapacity);

/**
 * @returns Frame size in bytes
 */
unsigned maxVoiceFrameSize(CODECType type);

unsigned codecSampleRate(CODECType type);

/**
 * @returns The number of samples in a frame.
 */
unsigned codecBlockSize(CODECType type);

enum IEType {
    IAX2_IE_CALLING_NUMBER = 0x02,
    IAX2_IE_CALLING_NAME = 0x04,
    IAX2_IE_USERNAME = 0x06,
    /** Actual CODEC capability */
    IAX2_IE_CAPABILITY = 0x08,
    IAX2_IE_FORMAT = 0x09,
    IAX2_IE_LANGUAGE = 0x0a,
    IAX2_IE_VERSION = 0x0b,
    IAX2_IE_AUTHMETHODS = 0x0e,
    IAX2_IE_CHALLENGE = 0x0f,
    IAX2_IE_MD5_RESULT = 0x10,
    IAX2_IE_APPARENT_ADDR = 0x12,
    IAX2_IE_CAUSE = 0x16,
    // NOTE: Not in IANA yet, working on getting this registered
    IAX2_IE_ED25519_RESULT = 0x20,
    // NOTE: Not in IANA yet
    IAX2_IE_TARGET_ADDR = 0x21,
    IAX2_IE_TARGET_ADDR2 = 0x22,
    IAX2_IE_CAUSECODE = 0x2a,
    IAX2_IE_CODEC_PREFS = 0x2d,
    // NOTE: NOT IN IANA! These are Asterisk's CAPABILITY2/FORMAT2: a
    // one-byte version (0) followed by a 64-bit big-endian bitfield.
    // IMPORTANT: Asterisk lets these override the 32-bit versions.
    IAX2_IE_CAPABILITY_WIDE = 0x37,
    IAX2_IE_FORMAT_WIDE = 0x38
};

/**
 * A strange kind of comparison that takes into account wrapping,
 * assuming that the two values are within 128 of each other.
 * For example, 0xfd < 0x04 because the RHS value is assumed to 
 * have just wrapped around.
 */
int compareSeqWrap(uint8_t a, uint8_t b);

/**
 * Converts the CODEC letter used in the CODEC_PREFS IE to a 32-bit CODEC mask.
 *
 * IMPORTANT: The letter is NOT the bit position of the CODEC. It is a 1-based
 * index (offset from 'A') into Asterisk's iax2_supported_formats[] table
 * (channels/iax2/codec_pref.c), whose order differs from the bitfield
 * order (ex: SLIN16 is 'P', ILBC is 'M').
 *
 * @returns The CODEC mask, or zero on fail (or if the CODEC can't be
 * represented in 32 bits).
 */
uint32_t codecLetterToMask(char letter);

/**
 * The inverse of codecLetterToMask().
 * @returns The letter, or 0 if the CODEC has no letter.
 */
char codecMaskToLetter(uint32_t mask);

/**
 * Builds a CODEC_PREFS string for the supported CODECs in our
 * preference order.
 * @returns The length of the string written (null-terminated).
 */
unsigned makeCodecPrefString(char* buf, unsigned bufCapacity);

/**
 * Converts a single CODEC to the value that goes in the one-byte
 * subclass of a full VOICE frame. Per Asterisk/RFC 5456 Section 8.1.1,
 * values >= 0x80 are sent as 0x80 | log2(codec).
 */
uint8_t compressCodecSubclass(uint32_t codec);

/**
 * The inverse of compressCodecSubclass().
 */
uint32_t uncompressCodecSubclass(uint8_t subclass);

/**
 * Takes the string that is returned in the CODEC_PREF information element (0x2b)
 * and parses it into a list of 32-bit CODEC masks in order of the expressed
 * preference.
 * @param prefList A null-terminated string of characters that represent the 
 * CODECs ("A offset").
 * @param codecs The array where the result will be written
 * @param codecsCapacity The space available in codecs.
 * @returns The number of CODECs in the resulting list.
 */
unsigned parseCodecPref(const char* prefList, uint32_t* codecs, unsigned codecsCapacity);

/**
 * Makes a decision about the CODEC that should be used for the call based 
 * on capabilities and preferences. The caller's preferences have priority.
 * @param callerCapability A mask with 1s for all the CODECs supported by the 
 * caller.
 * @param callerDesire A single CODEC that the caller would prefer.
 * @param calleeCapability A mask with 1s for all the CODECs supported by the 
 * callee.
 * @returns The CODEC that should be assigned, or zero if there was no assignment
 * possible.
 */
uint32_t assignCodec(uint32_t callerCapability, 
    uint32_t callerDesire,
    const uint32_t* callerPrefs, unsigned callerPrefsLen,
    uint32_t calleeCapability,
    const uint32_t* calleePrefs, unsigned calleePrefsLen);

/**
 * Provides the "wide" representation of the CODEC (version byte 0
 * followed by the 64-bit bitfield in network byte order).
 * @param buf Must be a 9-byte buffer.
 */
void fillCodecWide(uint32_t codecs, char* buf);

/**
 * Parses the "wide" CODEC representation (CAPABILITY2/FORMAT2 IEs).
 * Bits above 31 are dropped since we don't support any of those CODECs.
 * @returns true if the buffer was a valid version 0 representation.
 */
bool parseCodecWide(const uint8_t* buf, unsigned len, uint32_t* codecs);

/**
 * Parses the CALLING_NAME sent by an app using AllStarLink App Authentication
 * (ASL002). The format is TOKEN/CALLSIGN. A legacy WebTransceiver client 
 * sends only the TOKEN. The callsign is normalized to upper case.
 *
 * The callsign is restricted to [A-Za-z0-9]. The token may be any 
 * printable ASCII other than '/' (legacy WebTransceiver tokens come from 
 * the account's password hash), so it MUST be URL-escaped before use.
 *
 * @returns true if the value is well-formed.
 */
bool parseAppAuthCallingName(const char* callingName, 
    char* token, unsigned tokenCapacity,
    char* callsign, unsigned callsignCapacity);

/**
 * Interprets the body returned by the ASL API appauth/validate endpoint
 * using the same rules as the ASL3 [allstar-public] dialplan: the response 
 * must start with OHYES, followed by the callsign, which must match the 
 * callsign that was presented (case-insensitive) unless none was presented.
 *
 * @param validatedCallsign Receives the (upper case) callsign on success.
 * @returns true if the caller is validated.
 */
bool interpretAppAuthResponse(const char* response, const char* presentedCallsign,
    char* validatedCallsign, unsigned validatedCallsignCapacity);

/**
 * A structure that holds the result of a parse of an IAX URL
 */
struct IAXURIParameters {
    bool valid;
    char username[16];
    char hostname[64];
    int port;
    char number[16];
    char password[32];
};

/**
 * A utility function that parses an IAX URI.
 * 
 * Format is: 
 *
 * iax:[username@]hostname[:udp_port]/node_number,iax_password
 * 
 * The port, if omitted, is considered to be the same as the default, 4569.
 *
 * All URI components, except the username field, are case insensitive, and MUST be normalized 
 * to lower case as per Section 6.2.2.1 of [RFC3986].
 *
 */
IAXURIParameters parseIAXURI(const char* uri);

}
