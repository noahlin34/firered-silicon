// Charmap decoding: turns engine text bytes into something a human (or an
// agent) can read in a failure message.
//
// The engine never stores ASCII. Dialogue lives in charmap.txt's encoding --
// 'A' is 0xBB, 'é' is 0x1B, placeholders are multi-byte sequences ("PLAYER" is
// FD 01), lines end with FE and strings terminate with FF (EOS). A test that
// compares gStringVar4 against "RED received a TOWN MAP" compares bytes it can
// only get right by decoding, and a failing byte diff is unreadable without this.
//
// The table is read from the repository's own charmap.txt at startup, the same
// file tools/preproc consumes, so it cannot drift from the build.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

#define MAX_ENTRIES 1600
#define MAX_SEQ 4

struct CharmapEntry
{
    u8 bytes[MAX_SEQ];
    int len;
    char text[32];       // printable form, UTF-8 where the table is UTF-8
};

static struct CharmapEntry sEntries[MAX_ENTRIES];
static int sEntryCount;
static int sLoaded;

// Renders one charmap byte-sequence into `out`. Control codes get a visible
// spelling so a decoded string never contains a raw 0xFE.
static void AppendSequence(char *out, size_t outSize, int *used,
                           const u8 *bytes, int len)
{
    int i;

    for (i = 0; i < len; i++)
    {
        if (*used + 1 < (int)outSize)
            out[(*used)++] = (char)bytes[i];
    }
}

// Parses one hex byte pair at `s`; returns bytes consumed, or 0 on failure.
static int ReadHexByte(const char *s, u8 *out)
{
    char *end;
    long v;

    if (!((s[0] >= '0' && s[0] <= '9') || (s[0] >= 'A' && s[0] <= 'F')
        || (s[0] >= 'a' && s[0] <= 'f')))
        return 0;
    if (!((s[1] >= '0' && s[1] <= '9') || (s[1] >= 'A' && s[1] <= 'F')
        || (s[1] >= 'a' && s[1] <= 'f')))
        return 0;

    v = strtol(s, &end, 16);
    *out = (u8)v;
    return (int)(end - s);
}

// Reads the left-hand side of a charmap line. For a quoted character this copies
// its (already UTF-8) text; constants are rendered as {NAME} so the decoded
// string shows the placeholder rather than an opaque byte pair.
static int ParseLhs(const char *line, char *text, size_t textSize, int *isChar)
{
    const char *p = line;
    size_t n = 0;

    while (*p == ' ' || *p == '\t')
        p++;

    if (*p == '\'')
    {
        p++;
        if (*p == '\\')
            p++;                        // an escape such as '\l'
        while (*p != '\'' && *p != '\0')
        {
            if (n + 1 < textSize)
                text[n++] = *p;
            p++;
        }
        *isChar = 1;
    }
    else
    {
        // Constant: NAME = ...
        if (n + 1 < textSize)
            text[n++] = '{';
        while (*p != '=' && *p != '\0' && *p != ' ' && *p != '\t')
        {
            if (n + 1 < textSize)
                text[n++] = *p;
            p++;
        }
        if (n + 1 < textSize)
            text[n++] = '}';
        *isChar = 0;
    }

    text[n] = '\0';
    return 1;
}

static void LoadCharmap(void)
{
    FILE *fp;
    char line[512];

    if (sLoaded)
        return;
    sLoaded = 1;

    fp = fopen("charmap.txt", "r");
    if (fp == NULL)
    {
        // Not fatal: tests that do not decode text still work, and a decoded
        // comparison is reported as an obviously-wrong "(charmap unavailable)".
        fprintf(stderr, "[tests] warning: could not open charmap.txt; "
                        "text comparisons will not decode\n");
        return;
    }

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        char *eq;
        char lhstext[32];
        int isChar;
        u8 bytes[MAX_SEQ];
        int len = 0;
        const char *p;

        // Comments start at '@' and run to end of line.
        {
            char *at = strchr(line, '@');
            if (at != NULL)
                *at = '\0';
        }

        eq = strchr(line, '=');
        if (eq == NULL)
            continue;

        *eq = '\0';
        p = eq + 1;

        while (len < MAX_SEQ)
        {
            u8 b;
            int used;

            while (*p == ' ' || *p == '\t')
                p++;
            if (*p == '\0' || *p == '\n' || *p == '\r')
                break;

            used = ReadHexByte(p, &b);
            if (used == 0)
                break;
            bytes[len++] = b;
            p += used;
        }

        if (len == 0 || sEntryCount >= MAX_ENTRIES)
            continue;

        ParseLhs(line, lhstext, sizeof(lhstext), &isChar);
        if (lhstext[0] == '\0')
            continue;

        memcpy(sEntries[sEntryCount].bytes, bytes, len);
        sEntries[sEntryCount].len = len;
        snprintf(sEntries[sEntryCount].text, sizeof(sEntries[sEntryCount].text),
                 "%s", lhstext);
        sEntryCount++;
    }

    fclose(fp);
}

// Decode one symbol at `p`, preferring the CHARACTER interpretation.
//
// charmap.txt mixes two namespaces that overlap. A multi-byte control constant
// frequently begins with a byte that is also a standalone character:
//
//     'u'              = E9        SE_M_HEAT_WAVE   = E9 00
//     '0'              = A1        SE_M_MIST        = A1 00
//     '\u3000' (space) = 00        MUS_HEAL         = 00 01
//
// Dialogue contains "Yo" and "u" followed by a space, so a greedy longest-match
// picks SE_M_HEAT_WAVE for a plain letter and the decoded string fills with
// sound-effect names instead of text (this corrupted the first version of this
// decoder). The ambiguity is real in the table, and the writer's intent is only
// recoverable from context: text that a message box displays is characters, and
// the constants are emitted by different script commands (playse/playbgm) whose
// operands never reach a text printer.
//
// So: a single-byte character entry always wins. A multi-byte entry is only used
// when its FIRST byte is not itself a character -- which is exactly the
// placeholder family ({PLAYER} = FD 01, {STR_VAR_1} = FD 02, ...), where 0xFD is
// not a standalone character and therefore cannot be confused.
static int DecodeAt(const u8 *p, int remaining, char *buf, size_t bufSize)
{
    int i;

    // 1. A standalone character at this position always wins.
    for (i = 0; i < sEntryCount; i++)
    {
        const struct CharmapEntry *e = &sEntries[i];

        if (e->len == 1 && remaining >= 1 && p[0] == e->bytes[0])
        {
            snprintf(buf, bufSize, "%s", e->text);
            return 1;
        }
    }

    // 2. Otherwise the longest multi-byte entry whose first byte is unambiguous.
    {
        int best = -1;
        int bestLen = 0;

        for (i = 0; i < sEntryCount; i++)
        {
            const struct CharmapEntry *e = &sEntries[i];
            int firstIsChar = 0;
            int j;

            if (e->len < 2 || e->len > remaining || e->len <= bestLen)
                continue;

            for (j = 0; j < sEntryCount; j++)
            {
                if (sEntries[j].len == 1 && sEntries[j].bytes[0] == e->bytes[0])
                {
                    firstIsChar = 1;
                    break;
                }
            }
            if (firstIsChar)
                continue;
            if (memcmp(p, e->bytes, e->len) == 0)
            {
                best = i;
                bestLen = e->len;
            }
        }

        if (best < 0)
            return 0;

        snprintf(buf, bufSize, "%s", sEntries[best].text);
        return bestLen;
    }
}

const char *Test_Decode(const u8 *encoded)
{
    static char sOut[2048];
    static char sSeq[32];
    int used = 0;
    int i = 0;

    LoadCharmap();
    sOut[0] = '\0';

    if (encoded == NULL)
        return "(null)";
    if (!sLoaded || sEntryCount == 0)
    {
        snprintf(sOut, sizeof(sOut), "(charmap unavailable)");
        return sOut;
    }

    // Only 0xFF (EOS) terminates. 0x00 is NOT a terminator: it is the space
    // character (' ' = 00 in charmap.txt), so stopping there truncates every
    // multi-word string at its first space -- the most common case in dialogue,
    // and how this decoder originally reported "You" for a full sentence.
    //
    // A bounded scan, because an unwritten or unterminated buffer has no EOS at
    // all: reading on would walk off the end, and a zero-filled buffer decodes as
    // a wall of spaces that reads like a real (if bizarre) string. Naming that
    // case is worth the branch -- it costs a round of diagnosis otherwise.
    #define DECODE_SCAN_LIMIT 1024
    while (encoded[i] != 0xFF && i < DECODE_SCAN_LIMIT && used < (int)sizeof(sOut) - 1)
    {
        // The three layout escapes are stored in the table, but their printable
        // form is a marker rather than a control byte.
        switch (encoded[i])
        {
        case 0xFE:                      // '\n' new line
            if (used + 1 < (int)sizeof(sOut))
                sOut[used++] = '\n';
            i++;
            continue;
        case 0xFA:                      // '\l' scroll up
            used += snprintf(sOut + used, sizeof(sOut) - used, "<scroll>");
            i++;
            continue;
        case 0xFB:                      // '\p' new paragraph
            used += snprintf(sOut + used, sizeof(sOut) - used, "\n\n");
            i++;
            continue;
        default:
            break;
        }

        sSeq[0] = '\0';
        {
            int consumed = DecodeAt(&encoded[i], 8, sSeq, sizeof(sSeq));
            if (consumed == 0)
            {
                // Unknown byte: show it, never silently drop it.
                used += snprintf(sOut + used, sizeof(sOut) - used, "<%02X>", encoded[i]);
                i++;
                continue;
            }
            AppendSequence(sOut, sizeof(sOut), &used, (const u8 *)sSeq, (int)strlen(sSeq));
            i += consumed;
        }
    }

    if (encoded[i] != 0xFF)
    {
        // No EOS within the scan limit: the buffer was never written, or the
        // string is unterminated. Report that rather than returning the
        // space-filled prefix, which reads as a valid message.
        snprintf(sOut, sizeof(sOut), "(no EOS in %d bytes: buffer unwritten?)", i);
        return sOut;
    }

    sOut[used] = '\0';
    return sOut;
}

static char sLastDecoded[2048];

const char *Test_LastDecoded(void) { return sLastDecoded; }

const char *Test_StringVar4(void)
{
    const char *decoded = Test_Decode(gStringVar4);
    snprintf(sLastDecoded, sizeof(sLastDecoded), "%s", decoded);
    return sLastDecoded;
}
