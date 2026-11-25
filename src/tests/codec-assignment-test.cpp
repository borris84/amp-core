#include <iostream>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "IAX2Util.h"
#include "Transcoder_G722.h"

using namespace std;
using namespace kc1fsz;

// Unlike CHECK(), this is always evaluated (even with NDEBUG/Release builds)
#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #x); \
    exit(1); } } while (0)

static void testPrefLetters() {

    // These values come from Asterisk's iax2_supported_formats[] table
    // (channels/iax2/codec_pref.c). The letter is NOT the bit position.
    CHECK(codecLetterToMask('C') == CODECType::IAX2_CODEC_GSM_FULL);
    CHECK(codecLetterToMask('D') == CODECType::IAX2_CODEC_G711_ULAW);
    CHECK(codecLetterToMask('E') == CODECType::IAX2_CODEC_G711_ALAW);
    CHECK(codecLetterToMask('H') == CODECType::IAX2_CODEC_SLIN_8K);
    CHECK(codecLetterToMask('M') == 0x00000400); // ILBC
    CHECK(codecLetterToMask('N') == CODECType::IAX2_CODEC_G726_AAL2);
    CHECK(codecLetterToMask('O') == CODECType::IAX2_CODEC_G722);
    CHECK(codecLetterToMask('P') == CODECType::IAX2_CODEC_SLIN_16K);
    // SPEEX16 lives above bit 31
    CHECK(codecLetterToMask('L') == 0);
    CHECK(codecLetterToMask('A') == 0);
    CHECK(codecLetterToMask('z') == 0);

    CHECK(codecMaskToLetter(CODECType::IAX2_CODEC_G711_ULAW) == 'D');
    CHECK(codecMaskToLetter(CODECType::IAX2_CODEC_SLIN_8K) == 'H');
    CHECK(codecMaskToLetter(CODECType::IAX2_CODEC_SLIN_16K) == 'P');
    CHECK(codecMaskToLetter(CODECType::IAX2_CODEC_G726_AAL2) == 'N');
    CHECK(codecMaskToLetter(CODECType::IAX2_CODEC_G722) == 'O');
    // Not representable
    CHECK(codecMaskToLetter(CODECType::IAX2_CODEC_SLIN_48K) == 0);

    uint32_t result[3];
    CHECK(parseCodecPref("EDH", result, 3) == 3);
    CHECK(result[0] == CODECType::IAX2_CODEC_G711_ALAW);
    CHECK(result[1] == CODECType::IAX2_CODEC_G711_ULAW);
    CHECK(result[2] == CODECType::IAX2_CODEC_SLIN_8K);

    // Pre-2026-11 Ampersand nodes sent "QDF" meaning SLIN16, ULAW, G726_AAL2
    uint32_t legacy[3];
    CHECK(parseCodecPref("QDF", legacy, 3, true) == 3);
    CHECK(legacy[0] == CODECType::IAX2_CODEC_SLIN_16K);
    CHECK(legacy[1] == CODECType::IAX2_CODEC_G711_ULAW);
    CHECK(legacy[2] == CODECType::IAX2_CODEC_G726_AAL2);
    // ... which Asterisk would interpret as (nothing), ULAW, G726 
    CHECK(parseCodecPref("QDF", legacy, 3) == 2);

    // Legacy detection: the old 64-bit encoding of the 32-bit value
    const uint32_t legacyNarrow = 0x804c;
    const uint32_t legacyWide = 0x08000000 | 0x400 | 0x100 | 0x4;
    CHECK(isLegacyAmpersandWideCodec(legacyNarrow, legacyWide));
    CHECK(!isLegacyAmpersandWideCodec(legacyNarrow, legacyNarrow));

        // A typical ASL3 preference list: ulaw, adpcm, gsm, slin16
    uint32_t r2[8];
    CHECK(parseCodecPref("DGCP", r2, 8) == 4);
    CHECK(r2[3] == CODECType::IAX2_CODEC_SLIN_16K);
}

static void testWide() {
    // The 9-byte format is version 0 + 64-bit network byte order
    char buf[9];
    fillCodecWide(CODECType::IAX2_CODEC_SLIN_16K, buf);
    const uint8_t expSlin16[9] = { 0, 0, 0, 0, 0, 0, 0, 0x80, 0x00 };
    CHECK(memcmp(buf, expSlin16, 9) == 0);

    fillCodecWide(CODECType::IAX2_CODEC_SLIN_8K, buf);
    const uint8_t expSlin8[9] = { 0, 0, 0, 0, 0, 0, 0, 0x00, 0x40 };
    CHECK(memcmp(buf, expSlin8, 9) == 0);

    fillCodecWide(CODECType::IAX2_CODEC_G722, buf);
    const uint8_t expG722[9] = { 0, 0, 0, 0, 0, 0, 0, 0x10, 0x00 };
    CHECK(memcmp(buf, expG722, 9) == 0);

    const uint32_t all = CODECType::IAX2_CODEC_G711_ULAW | CODECType::IAX2_CODEC_SLIN_8K |
        CODECType::IAX2_CODEC_SLIN_16K | CODECType::IAX2_CODEC_G726_AAL2 |
        CODECType::IAX2_CODEC_G722;
    fillCodecWide(all, buf);
    uint32_t back = 0;
    CHECK(parseCodecWide((const uint8_t*)buf, 9, &back));
    CHECK(back == all);

    // Wrong version/length
    uint8_t bad[9] = { 1, 0, 0, 0, 0, 0, 0, 0, 4 };
    CHECK(!parseCodecWide(bad, 9, &back));
    CHECK(!parseCodecWide(bad, 8, &back));
}

static void testSubclass() {
    CHECK(compressCodecSubclass(CODECType::IAX2_CODEC_G711_ULAW) == 0x04);
    CHECK(compressCodecSubclass(CODECType::IAX2_CODEC_SLIN_8K) == 0x40);
    CHECK(compressCodecSubclass(CODECType::IAX2_CODEC_G722) == 0x8c);
    CHECK(compressCodecSubclass(CODECType::IAX2_CODEC_SLIN_16K) == 0x8f);
    CHECK(uncompressCodecSubclass(0x8f) == CODECType::IAX2_CODEC_SLIN_16K);
    CHECK(uncompressCodecSubclass(0x8c) == CODECType::IAX2_CODEC_G722);
    CHECK(uncompressCodecSubclass(0x04) == CODECType::IAX2_CODEC_G711_ULAW);
}

static void testPrefsAndOptional() {
    char p[16];
    // G.722 is optional and off by default
    CHECK(!isCodecSupported(CODECType::IAX2_CODEC_G722));
    makeCodecPrefString(p, sizeof(p));
    CHECK(strcmp(p, "PHDN") == 0);

    setCodecEnabled(CODECType::IAX2_CODEC_G722, true);
    CHECK(isCodecSupported(CODECType::IAX2_CODEC_G722));
    makeCodecPrefString(p, sizeof(p));
    CHECK(strcmp(p, "POHDN") == 0);

    // A caller that only does G.722 + ulaw and asks for G.722
    uint32_t ours[8];
    unsigned oursLen = getCodecPrefs(ours, 8);
    CHECK(assignCodec(CODECType::IAX2_CODEC_G722 | CODECType::IAX2_CODEC_G711_ULAW,
        CODECType::IAX2_CODEC_G722, 0, 0, getSupportedCodecs(), ours, oursLen) ==
        CODECType::IAX2_CODEC_G722);

    // Base CODECs can't be turned off
    setCodecEnabled(CODECType::IAX2_CODEC_G711_ULAW, false);
    CHECK(isCodecSupported(CODECType::IAX2_CODEC_G711_ULAW));

    setCodecEnabled(CODECType::IAX2_CODEC_G722, false);
    CHECK(!isCodecSupported(CODECType::IAX2_CODEC_G722));
    CHECK(maxVoiceFrameSize(CODECType::IAX2_CODEC_G722) == 160);
    CHECK(codecBlockSize(CODECType::IAX2_CODEC_G722) == 320);
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

    CHECK(assignCodec(callerCapability, 0, callerPrefs, 2,
        calleeCapability, calleePrefs, 1) == CODECType::IAX2_CODEC_G711_ULAW);
}

static void testAppAuth() {
    char token[65], cs[65];
    const char* sha = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    char name[100];
    snprintf(name, sizeof(name), "%s/n0call", sha);
    CHECK(parseAppAuthCallingName(name, token, sizeof(token), cs, sizeof(cs)));
    CHECK(strcmp(token, sha) == 0);
    CHECK(strcmp(cs, "N0CALL") == 0);

    // Legacy WebTransceiver: token only
    CHECK(parseAppAuthCallingName("abcdefABCDEF", token, sizeof(token), cs, sizeof(cs)));
    CHECK(strcmp(token, "abcdefABCDEF") == 0);
    CHECK(cs[0] == 0);

    // Legacy tokens come from a password hash and may have punctuation
    CHECK(parseAppAuthCallingName("a$b.c+d=e1F2", token, sizeof(token), cs, sizeof(cs)));
    CHECK(strcmp(token, "a$b.c+d=e1F2") == 0);
    CHECK(cs[0] == 0);

    // Malformed: empty, whitespace, bad callsign, too long
    CHECK(!parseAppAuthCallingName("", token, sizeof(token), cs, sizeof(cs)));
    CHECK(!parseAppAuthCallingName("/N0CALL", token, sizeof(token), cs, sizeof(cs)));
    CHECK(!parseAppAuthCallingName("abc def/N0CALL", token, sizeof(token), cs, sizeof(cs)));
    CHECK(!parseAppAuthCallingName("abc/N0CALL&x", token, sizeof(token), cs, sizeof(cs)));
    CHECK(!parseAppAuthCallingName("abc/N0/CALL", token, sizeof(token), cs, sizeof(cs)));
    snprintf(name, sizeof(name), "%sX/N0CALL", sha);
    CHECK(!parseAppAuthCallingName(name, token, sizeof(token), cs, sizeof(cs)));

    // Responses from the API
    char v[65];
    CHECK(interpretAppAuthResponse("OHYESN0CALL", "N0CALL", v, sizeof(v)));
    CHECK(strcmp(v, "N0CALL") == 0);
    CHECK(interpretAppAuthResponse("OHYESN0CALL\n", "n0call", v, sizeof(v)));
    CHECK(!interpretAppAuthResponse("OHYESW1AW", "N0CALL", v, sizeof(v)));
    CHECK(!interpretAppAuthResponse("OHYESN0CALLX", "N0CALL", v, sizeof(v)));
    CHECK(!interpretAppAuthResponse("", "N0CALL", v, sizeof(v)));
    CHECK(!interpretAppAuthResponse("?", "N0CALL", v, sizeof(v)));
    CHECK(!interpretAppAuthResponse("OHYES", "", v, sizeof(v)));
    // Legacy client: we take the API's callsign
    CHECK(interpretAppAuthResponse("OHYESw1aw", "", v, sizeof(v)));
    CHECK(strcmp(v, "W1AW") == 0);
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
        CHECK(tc.encode(in + b * n, n, code, sizeof(code)));
        CHECK(tc.decode(code, sizeof(code), out + b * n, n));
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
    CHECK(bestSnr > 20.0);

    // Size validation
    uint8_t code[10];
    CHECK(!tc.encode(in, n, code, sizeof(code)));
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
