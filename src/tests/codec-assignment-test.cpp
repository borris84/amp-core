#include <iostream>
#include <cassert>
#include <cstring>
#include <cmath>

#include "IAX2Util.h"
#include "Transcoder_G722.h"

using namespace std;
using namespace kc1fsz;

static void testPrefLetters() {

    // These values come from Asterisk's iax2_supported_formats[] table
    // (channels/iax2/codec_pref.c). The letter is NOT the bit position.
    assert(codecLetterToMask('C') == CODECType::IAX2_CODEC_GSM_FULL);
    assert(codecLetterToMask('D') == CODECType::IAX2_CODEC_G711_ULAW);
    assert(codecLetterToMask('E') == CODECType::IAX2_CODEC_G711_ALAW);
    assert(codecLetterToMask('H') == CODECType::IAX2_CODEC_SLIN_8K);
    assert(codecLetterToMask('M') == 0x00000400); // ILBC
    assert(codecLetterToMask('N') == CODECType::IAX2_CODEC_G726_AAL2);
    assert(codecLetterToMask('O') == CODECType::IAX2_CODEC_G722);
    assert(codecLetterToMask('P') == CODECType::IAX2_CODEC_SLIN_16K);
    // SPEEX16 lives above bit 31
    assert(codecLetterToMask('L') == 0);
    assert(codecLetterToMask('A') == 0);
    assert(codecLetterToMask('z') == 0);

    assert(codecMaskToLetter(CODECType::IAX2_CODEC_G711_ULAW) == 'D');
    assert(codecMaskToLetter(CODECType::IAX2_CODEC_SLIN_8K) == 'H');
    assert(codecMaskToLetter(CODECType::IAX2_CODEC_SLIN_16K) == 'P');
    assert(codecMaskToLetter(CODECType::IAX2_CODEC_G726_AAL2) == 'N');
    assert(codecMaskToLetter(CODECType::IAX2_CODEC_G722) == 'O');
    // Not representable
    assert(codecMaskToLetter(CODECType::IAX2_CODEC_SLIN_48K) == 0);

    uint32_t result[3];
    assert(parseCodecPref("EDH", result, 3) == 3);
    assert(result[0] == CODECType::IAX2_CODEC_G711_ALAW);
    assert(result[1] == CODECType::IAX2_CODEC_G711_ULAW);
    assert(result[2] == CODECType::IAX2_CODEC_SLIN_8K);

    // A typical ASL3 preference list: ulaw, adpcm, gsm, slin16
    uint32_t r2[8];
    assert(parseCodecPref("DGCP", r2, 8) == 4);
    assert(r2[3] == CODECType::IAX2_CODEC_SLIN_16K);
}

static void testWide() {
    // The 9-byte format is version 0 + 64-bit network byte order
    char buf[9];
    fillCodecWide(CODECType::IAX2_CODEC_SLIN_16K, buf);
    const uint8_t expSlin16[9] = { 0, 0, 0, 0, 0, 0, 0, 0x80, 0x00 };
    assert(memcmp(buf, expSlin16, 9) == 0);

    fillCodecWide(CODECType::IAX2_CODEC_SLIN_8K, buf);
    const uint8_t expSlin8[9] = { 0, 0, 0, 0, 0, 0, 0, 0x00, 0x40 };
    assert(memcmp(buf, expSlin8, 9) == 0);

    fillCodecWide(CODECType::IAX2_CODEC_G722, buf);
    const uint8_t expG722[9] = { 0, 0, 0, 0, 0, 0, 0, 0x10, 0x00 };
    assert(memcmp(buf, expG722, 9) == 0);

    const uint32_t all = CODECType::IAX2_CODEC_G711_ULAW | CODECType::IAX2_CODEC_SLIN_8K |
        CODECType::IAX2_CODEC_SLIN_16K | CODECType::IAX2_CODEC_G726_AAL2 |
        CODECType::IAX2_CODEC_G722;
    fillCodecWide(all, buf);
    uint32_t back = 0;
    assert(parseCodecWide((const uint8_t*)buf, 9, &back));
    assert(back == all);

    // Wrong version/length
    uint8_t bad[9] = { 1, 0, 0, 0, 0, 0, 0, 0, 4 };
    assert(!parseCodecWide(bad, 9, &back));
    assert(!parseCodecWide(bad, 8, &back));
}

static void testSubclass() {
    assert(compressCodecSubclass(CODECType::IAX2_CODEC_G711_ULAW) == 0x04);
    assert(compressCodecSubclass(CODECType::IAX2_CODEC_SLIN_8K) == 0x40);
    assert(compressCodecSubclass(CODECType::IAX2_CODEC_G722) == 0x8c);
    assert(compressCodecSubclass(CODECType::IAX2_CODEC_SLIN_16K) == 0x8f);
    assert(uncompressCodecSubclass(0x8f) == CODECType::IAX2_CODEC_SLIN_16K);
    assert(uncompressCodecSubclass(0x8c) == CODECType::IAX2_CODEC_G722);
    assert(uncompressCodecSubclass(0x04) == CODECType::IAX2_CODEC_G711_ULAW);
}

static void testPrefsAndOptional() {
    char p[16];
    // G.722 is optional and off by default
    assert(!isCodecSupported(CODECType::IAX2_CODEC_G722));
    makeCodecPrefString(p, sizeof(p));
    assert(strcmp(p, "PHDN") == 0);

    setCodecEnabled(CODECType::IAX2_CODEC_G722, true);
    assert(isCodecSupported(CODECType::IAX2_CODEC_G722));
    makeCodecPrefString(p, sizeof(p));
    assert(strcmp(p, "POHDN") == 0);

    // A caller that only does G.722 + ulaw and asks for G.722
    uint32_t ours[8];
    unsigned oursLen = getCodecPrefs(ours, 8);
    assert(assignCodec(CODECType::IAX2_CODEC_G722 | CODECType::IAX2_CODEC_G711_ULAW,
        CODECType::IAX2_CODEC_G722, 0, 0, getSupportedCodecs(), ours, oursLen) ==
        CODECType::IAX2_CODEC_G722);

    // Base CODECs can't be turned off
    setCodecEnabled(CODECType::IAX2_CODEC_G711_ULAW, false);
    assert(isCodecSupported(CODECType::IAX2_CODEC_G711_ULAW));

    setCodecEnabled(CODECType::IAX2_CODEC_G722, false);
    assert(!isCodecSupported(CODECType::IAX2_CODEC_G722));
    assert(maxVoiceFrameSize(CODECType::IAX2_CODEC_G722) == 160);
    assert(codecBlockSize(CODECType::IAX2_CODEC_G722) == 320);
}

static void testAssignment() {
    // Preference tests.
    // In this test the only place the caller and the callee share a capability
    // is with the G711_ULAW CODEC.
    uint32_t callerPrefs[2];
    callerPrefs[0] = CODECType::IAX2_CODEC_SLIN_8K;
    callerPrefs[1] = CODECType::IAX2_CODEC_G711_ULAW;
    uint32_t callerCapability = CODECType::IAX2_CODEC_SLIN_8K |
        CODECType::IAX2_CODEC_G711_ULAW;

    uint32_t calleePrefs[1];
    calleePrefs[0] = CODECType::IAX2_CODEC_SLIN_16K;
    uint32_t calleeCapability = CODECType::IAX2_CODEC_G711_ULAW |
        CODECType::IAX2_CODEC_SLIN_16K;

    assert(assignCodec(callerCapability, 0, callerPrefs, 2,
        calleeCapability, calleePrefs, 1) == CODECType::IAX2_CODEC_G711_ULAW);
}

static void testAppAuth() {
    char token[65], cs[65];
    const char* sha = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    char name[100];
    snprintf(name, sizeof(name), "%s/n0call", sha);
    assert(parseAppAuthCallingName(name, token, sizeof(token), cs, sizeof(cs)));
    assert(strcmp(token, sha) == 0);
    assert(strcmp(cs, "N0CALL") == 0);

    // Legacy WebTransceiver: token only
    assert(parseAppAuthCallingName("abcdefABCDEF", token, sizeof(token), cs, sizeof(cs)));
    assert(strcmp(token, "abcdefABCDEF") == 0);
    assert(cs[0] == 0);

    // Legacy tokens come from a password hash and may have punctuation
    assert(parseAppAuthCallingName("a$b.c+d=e1F2", token, sizeof(token), cs, sizeof(cs)));
    assert(strcmp(token, "a$b.c+d=e1F2") == 0);
    assert(cs[0] == 0);

    // Malformed: empty, whitespace, bad callsign, too long
    assert(!parseAppAuthCallingName("", token, sizeof(token), cs, sizeof(cs)));
    assert(!parseAppAuthCallingName("/N0CALL", token, sizeof(token), cs, sizeof(cs)));
    assert(!parseAppAuthCallingName("abc def/N0CALL", token, sizeof(token), cs, sizeof(cs)));
    assert(!parseAppAuthCallingName("abc/N0CALL&x", token, sizeof(token), cs, sizeof(cs)));
    assert(!parseAppAuthCallingName("abc/N0/CALL", token, sizeof(token), cs, sizeof(cs)));
    snprintf(name, sizeof(name), "%sX/N0CALL", sha);
    assert(!parseAppAuthCallingName(name, token, sizeof(token), cs, sizeof(cs)));

    // Responses from the API
    char v[65];
    assert(interpretAppAuthResponse("OHYESN0CALL", "N0CALL", v, sizeof(v)));
    assert(strcmp(v, "N0CALL") == 0);
    assert(interpretAppAuthResponse("OHYESN0CALL\n", "n0call", v, sizeof(v)));
    assert(!interpretAppAuthResponse("OHYESW1AW", "N0CALL", v, sizeof(v)));
    assert(!interpretAppAuthResponse("OHYESN0CALLX", "N0CALL", v, sizeof(v)));
    assert(!interpretAppAuthResponse("", "N0CALL", v, sizeof(v)));
    assert(!interpretAppAuthResponse("?", "N0CALL", v, sizeof(v)));
    assert(!interpretAppAuthResponse("OHYES", "", v, sizeof(v)));
    // Legacy client: we take the API's callsign
    assert(interpretAppAuthResponse("OHYESw1aw", "", v, sizeof(v)));
    assert(strcmp(v, "W1AW") == 0);
}

static void testG722() {
    // Round-trip a 1 kHz tone at 16k through G.722 and make sure that
    // it comes back (after the codec delay) with a reasonable SNR.
    Transcoder_G722 tc;
    const unsigned blocks = 25;
    const unsigned n = 320;
    int16_t in[blocks * n], out[blocks * n];
    for (unsigned i = 0; i < blocks * n; i++)
        in[i] = (int16_t)(8000.0 * sin(2.0 * M_PI * 1000.0 * i / 16000.0));
    for (unsigned b = 0; b < blocks; b++) {
        uint8_t code[160];
        assert(tc.encode(in + b * n, n, code, sizeof(code)));
        assert(tc.decode(code, sizeof(code), out + b * n, n));
    }
    // Find the delay that best lines things up
    double bestSnr = -1000;
    for (unsigned d = 0; d < 64; d++) {
        double sig = 0, err = 0;
        // Skip the first few blocks while the ADPCM adapts
        for (unsigned i = 5 * n; i < blocks * n - d; i++) {
            sig += (double)in[i] * in[i];
            double e = (double)out[i + d] - in[i];
            err += e * e;
        }
        double snr = 10.0 * log10(sig / (err + 1));
        if (snr > bestSnr)
            bestSnr = snr;
    }
    cout << "G.722 round-trip SNR " << bestSnr << " dB" << endl;
    assert(bestSnr > 20.0);

    // Size validation
    uint8_t code[10];
    assert(!tc.encode(in, n, code, sizeof(code)));
}

int main(int,const char**) {
    testPrefLetters();
    testWide();
    testSubclass();
    testPrefsAndOptional();
    testAssignment();
    testAppAuth();
    testG722();
    cout << "All codec tests passed" << endl;
    return 0;
}
