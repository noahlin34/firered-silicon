// Script inspection helpers.
//
// Native scripts are pret's bytecode compiled to byte arrays (tools/gen_map_data.py,
// tools/gen_battle_data.py). Pointer operands are stored as u32 INDICES into
// gNativeScriptPtrs rather than addresses, because the linker requires aligned
// pointer relocations (AGENTS.md fix #23) -- so reading one is a two-step
// operation that every script test would otherwise hand-roll, with the byte-order
// arithmetic repeated and easy to get wrong.
//
// Deliberately semantic rather than byte-pinning: tests should assert that a
// script *is* a real script doing the right thing, not that it currently encodes
// a particular operand layout. An assertion on raw operand bytes fails whenever
// the generator's encoding legitimately changes, which trains people to re-pin
// numbers rather than read the failure. See AGENTS.md's testing guidance.

#include <string.h>

#include "internal.h"

#include "constants/event_objects.h"
#include "script.h"

extern const void *const gNativeScriptPtrs[];

// Reads the 4-byte operand at `offset` and resolves it through the pointer table.
// This is what every pointer-taking script opcode means.
const u8 *Test_ScriptPtrAt(const u8 *script, int offset)
{
    u32 index;

    if (script == NULL)
        return NULL;

    index = (u32)script[offset]
          | ((u32)script[offset + 1] << 8)
          | ((u32)script[offset + 2] << 16)
          | ((u32)script[offset + 3] << 24);

    return (const u8 *)gNativeScriptPtrs[index];
}

// The same for non-pointer operands (u16 vars, item ids, quantities).
u16 Test_ScriptHalfwordAt(const u8 *script, int offset)
{
    if (script == NULL)
        return 0;
    return (u16)(script[offset] | (script[offset + 1] << 8));
}

u32 Test_ScriptWordAt(const u8 *script, int offset)
{
    if (script == NULL)
        return 0;
    return (u32)script[offset]
         | ((u32)script[offset + 1] << 8)
         | ((u32)script[offset + 2] << 16)
         | ((u32)script[offset + 3] << 24);
}

// Byte-sequence search inside a script.
//
// This is a BOUNDED BYTE SCAN, not a decode: the test layer has no operand-width
// table, so it cannot walk the script command by command. That is sufficient for
// presence checks and deliberately tolerant, but it means two things:
//
//   * the scan window is fixed rather than "stop at the first 0x02". 0x02 is
//     `end` as an opcode but also a legitimate operand byte (the quantity in
//     checkitemspace, for instance), so stopping there truncates the scan early
//     and reports a false negative -- which is how this was written the first
//     time, and what the full-window scan below replaces;
//   * a match may in principle span an operand boundary. Assertions built on this
//     should therefore describe behaviour ("this script contains the additem
//     that grants the item"), not pin an exact byte layout.
bool8 Test_ScriptContains(const u8 *script, const u8 *pattern, int patternLen)
{
    int i, j;

    if (script == NULL || pattern == NULL || patternLen <= 0)
        return FALSE;

    for (i = 0; i < TEST_SCRIPT_SCAN_LIMIT - patternLen; i++)
    {
        for (j = 0; j < patternLen; j++)
        {
            if (script[i + j] != pattern[j])
                break;
        }
        if (j == patternLen)
            return TRUE;
    }
    return FALSE;
}

// A single-opcode check: is this script's FIRST operation `opcode`?
bool8 Test_ScriptStartsWith(const u8 *script, u8 opcode)
{
    return script != NULL && script[0] == opcode;
}

// Locates the first occurrence of `opcode`, or -1. Same byte-scan caveat as
// above: this can match an operand byte that happens to equal `opcode`.
int Test_ScriptFindOpcode(const u8 *script, u8 opcode)
{
    int i;

    if (script == NULL)
        return -1;

    for (i = 0; i < TEST_SCRIPT_SCAN_LIMIT; i++)
    {
        if (script[i] == opcode)
            return i;
    }
    return -1;
}

// Reports a script's bytes. Used in failure messages so a broken script is
// diagnosable from the test output alone.
const char *Test_ScriptDump(const u8 *script, int maxBytes)
{
    static char sOut[512];
    int i, used = 0;

    if (script == NULL)
        return "(NULL)";

    for (i = 0; i < maxBytes && used < (int)sizeof(sOut) - 4; i++)
    {
        used += snprintf(sOut + used, sizeof(sOut) - used, "%02X ", script[i]);
        if (i > 0 && script[i] == 0x02)
            break;
    }
    return sOut;
}
