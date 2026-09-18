#!/usr/bin/env python3
"""Compile the game's audio data into native C for the macOS port.

The ROM build gets its audio data from the GBA assembler: `data/sound_data.s`
pulls in `sound/voice_groups.inc` and friends and assembles them into GBA
structs, and `audio_rules.mk` runs `mid2agb` over each `sound/songs/midi/*.mid`
to produce the song bytecode. Neither can run for the host:

  * the assembler emits 32-bit `.4byte` operands, which truncate on 64-bit
    macOS, and
  * the GBA struct layouts differ from the host's wherever a pointer is
    involved (`sizeof(struct ToneData)` is 12 on the GBA, 24 here).

So this generator is the audio counterpart of `tools/gen_map_data.py` and
`tools/gen_battle_data.py`. It reads the same pret sources and emits C with
host-width pointers, replacing every pointer operand with a u32 **index** into
an aligned pointer table (`gNativeSongPtrs`) -- the treatment fixes #23, #38
and #49 already use for map scripts, battle scripts and field effects.

Generated (tracked in git, like `src/data/maps_data.h`):

  src/data/sound/voice_data.h    voicegroups, keysplit tables, programmable
                                 waves, sample blobs (INCBIN)
  src/data/sound/song_data.h     song bytecode, headers, pointer index table
  src/data/sound/sound_data.c    gSongTable, gMPlayTable, track buffers,
                                 gCryTable / gCryTable_Reverse

The sample `.bin` files are NOT written here: `audio_rules.mk`'s existing
`wav2agb` rules build them, and this generator only emits the INCBINs that
reference them -- the same split the graphics assets use.

Two layout facts drive the emission, and both are traps:

  * `struct WaveData` is all scalars (u16 type, u16 status, u32 freq, u32
    loopStart, u32 size, s8 data[]), so its layout is identical on the host
    and a raw `wav2agb` blob can be cast straight to `struct WaveData *`.

  * `struct ToneData` is not. `voice_keysplit`/`voice_keysplit_all` pack a
    keysplit table pointer into the four `attack`/`decay`/`sustain`/`release`
    bytes, which is exactly 4 bytes -- enough for a GBA address, not for a host
    one. The port reads `ToneData.keySplitTable` instead (a PORTABLE-only
    field; see include/gba/m4a_internal.h).

This script must not invoke make (it can be run from inside the Makefile).
"""

import os
import re
import subprocess
import sys
from collections import OrderedDict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

MID2AGB = "tools/mid2agb/mid2agb"
SONG_TABLE = "sound/song_table.inc"
VOICE_GROUPS = "sound/voice_groups.inc"
KEYSPLIT_TABLES = "sound/keysplit_tables.inc"
PROG_WAVES = "sound/programmable_wave_data.inc"
DIRECT_SAMPLES = "sound/direct_sound_data.inc"
CRY_TABLES = "sound/cry_tables.inc"
MIDI_CFG = "sound/songs/midi/midi.cfg"
MPLAYDEF = "sound/MPlayDef.s"
MIDI_DIR = "sound/songs/midi"

OUT_VOICES = "src/data/sound/voice_data.h"
OUT_SONGS = "src/data/sound/song_data.h"
OUT_SOURCE = "src/data/sound/sound_data.c"

SONG_CACHE = "/tmp/gen_sound_data_songs"

# Song bytecode opcodes whose 4-byte operand is an address. mid2agb emits these
# as `.byte OP` + `.word label`, and the interpreter's handler reads the four
# operand bytes at `cmdPtr` (which MPlayMain has already advanced past the
# opcode). Verified against every song: 558 GOTO + 2530 PATT, no REPT, and no
# other opcode is ever followed by a `.word`.
POINTER_OPCODES = {0xB2: "GOTO", 0xB3: "PATT", 0xB5: "REPT"}

# Voice macros from asm/macros/music_voice.inc. Every one expands to exactly
# 12 bytes: `type`, `key`, `length`, `pan_sweep`, a 4-byte payload, then
# attack/decay/sustain/release. The payload is a host pointer for PCM and
# programmable-wave voices, but for the CGB voices it is the sweep / duty-cycle
# / noise-period byte followed by three zero bytes -- hence `length_arg` and
# `value_mask`, which reproduce those bytes.
#
#   name: (type_byte, length_arg_index, payload_arg_index, payload_mask)
#
# `None` for a length index or mask means that byte is zero in the macro.
VOICE_MACROS = {
    "voice_directsound":             (0x00, None, 2, None),
    "voice_directsound_no_resample": (0x08, None, 2, None),
    "voice_directsound_alt":         (0x10, None, 2, None),
    "voice_square_1":                (0x01, 2, 3, 0x3),
    "voice_square_1_alt":            (0x09, 2, 3, 0x3),
    "voice_square_2":                (0x02, None, 2, 0x3),
    "voice_square_2_alt":            (0x0A, None, 2, 0x3),
    "voice_noise":                   (0x04, None, 2, 0x1),
    "voice_noise_alt":               (0x0C, None, 2, 0x1),
    "voice_programmable_wave":       (0x03, None, 2, None),
    "voice_programmable_wave_alt":   (0x0B, None, 2, None),
}

# Macros whose payload is an address rather than a scalar byte.
POINTER_PAYLOAD = {"voice_directsound", "voice_directsound_no_resample",
                   "voice_directsound_alt", "voice_programmable_wave",
                   "voice_programmable_wave_alt"}

VOICE_GROUP_RE = re.compile(r"^voicegroup\d+$")
SAMPLE_PREFIXES = ("voicegroup", "DirectSoundWaveData_", "Cry_",
                   "ProgrammableWaveData_", "KeySplitTable")


def warn(msg):
    print("gen_sound_data: %s" % msg, file=sys.stderr)


def read(path):
    with open(path, encoding="utf-8", errors="replace") as handle:
        return handle.read()


def strip_comment(line):
    """`@` starts a comment in the pret assembler dialect used by these files."""
    return line.split("@")[0]


class ExprError(Exception):
    pass


def parse_equ_table(text):
    """`.equ NAME, VALUE` into a dict of *expression strings*.

    Kept unresolved because MPlayDef.s builds names on earlier ones
    (`W01, W00+1`) and the song files build on MPlayDef (`mus_x_rev,
    reverb_set+50`).
    """
    out = OrderedDict()
    for raw in text.splitlines():
        match = re.match(r"\.(?:equ|set)\s+(\w+)\s*,\s*(.+?)\s*$",
                         strip_comment(raw).strip())
        if match:
            out[match.group(1)] = match.group(2)
    return out


def eval_expr(expr, names, depth=0):
    """Evaluate an assembler integer expression.

    Handles what the sources actually use: decimal, 0x hex, `+ - * /`,
    parentheses, and previously-defined `.equ` names. Division is integer
    division, matching GNU as.
    """
    if depth > 64:
        raise ExprError("recursion limit on %r" % expr)
    text = expr.strip()
    if not text:
        raise ExprError("empty expression")

    def replace(match):
        token = match.group(0)
        if re.fullmatch(r"0[xX][0-9a-fA-F]+|\d+", token):
            return token
        if token in names:
            return "(%s)" % eval_expr(names[token], names, depth + 1)
        raise ExprError("unknown symbol %r" % token)

    substituted = re.sub(r"0[xX][0-9a-fA-F]+|\d+|[A-Za-z_]\w*", replace, text)
    # Every identifier is gone after substitution, so anything left must be a
    # number, an operator or a parenthesis. Hex literals keep their 0x prefix,
    # which is why the check is per-token rather than a character class.
    remainder = re.sub(r"0[xX][0-9a-fA-F]+|\d+", "0", substituted)
    if not re.fullmatch(r"[0+\-*/() ]+", remainder):
        raise ExprError("bad expression %r -> %r" % (expr, substituted))
    try:
        value = eval(substituted, {"__builtins__": {}}, {})  # noqa: S307 - numerically validated above
    except Exception as exc:
        raise ExprError("%s in %r" % (exc, expr))
    return int(value)


# --------------------------------------------------------------------------
# Voicegroups
# --------------------------------------------------------------------------

def resolve_symbol(expr, names, depth=0):
    """Resolve an operand that names a symbol rather than a number.

    mid2agb emits the song header's voicegroup as `.equ mus_x_grp, voicegroup159`
    and then `.word mus_x_grp`, so the operand is an alias chain ending in a real
    symbol (a voicegroup, a sample or a keysplit table). Numeric `.equ`s are not
    valid here and are reported rather than silently taken as a name.
    """
    if depth > 16:
        raise ExprError("symbol recursion on %r" % expr)
    token = expr.strip()
    if token in names:
        return resolve_symbol(names[token], names, depth + 1)
    if not re.fullmatch(r"[A-Za-z_]\w*", token):
        raise ExprError("%r does not name a symbol" % expr)
    return token


def parse_voice_groups(text):
    """OrderedDict(voicegroup -> [(macro, [args])])."""
    groups = OrderedDict()
    current = None
    for raw in text.splitlines():
        line = strip_comment(raw).strip()
        if not line:
            continue
        label = re.match(r"^(voicegroup\d+)::\s*$", line)
        if label:
            current = label.group(1)
            groups[current] = []
            continue
        if current is None:
            continue
        macro = re.match(r"^(voice_\w+)\s*(.*)$", line)
        if macro:
            args = [a.strip() for a in macro.group(2).split(",")]
            groups[current].append((macro.group(1), args))
    return groups


def pan_expr(pan):
    """`voice_*` pan byte: 0 means centre, otherwise the macro sets bit 7."""
    pan = pan.strip()
    return "0" if pan in ("0", "") else "0x80 | %s" % pan


def voice_initializer(macro, args):
    """One `struct ToneData` initializer for a voice macro.

    Mirrors the macro's exact byte emission: type, key, length, pan_sweep,
    payload (pointer or the CGB sweep/duty/period byte), then the four ADSR
    bytes. PCM and programmable-wave voices store a pointer, so their ADSR
    bytes pass through unmasked (the macros do not mask them either); the CGB
    macros mask each one.
    """
    if macro not in VOICE_MACROS:
        raise SystemExit("unknown voice macro %s" % macro)
    vtype, length_arg, payload_arg, mask = VOICE_MACROS[macro]
    key = args[0]
    pan = pan_expr(args[1])
    length = args[length_arg] if length_arg is not None else "0"
    payload = args[payload_arg]
    adsr = args[payload_arg + 1:payload_arg + 5]

    if macro in POINTER_PAYLOAD:
        return ("    { 0x%02X, %s, %s, %s, (struct WaveData *)%s, "
                "%s, %s, %s, %s },"
                % (vtype, key, length, pan, payload,
                   adsr[0], adsr[1], adsr[2], adsr[3]))

    # CGB voice: the payload byte is scalar, so `wav` is NULL.
    value = payload if mask is None else "(%s & 0x%X)" % (payload, mask)
    masked = ["(%s & 0x%X)" % (a, m) for a, m in zip(adsr, (0x7, 0x7, 0xF, 0x7))]
    return ("    { 0x%02X, %s, %s, %s, NULL, %s, %s, %s, %s },"
            % (vtype, key, length, pan,
               masked[0], masked[1], masked[2], masked[3]))


def emit_voice_groups(groups, out):
    out.append("/*\n"
               " * Voicegroups: one `struct ToneData` per voice. The GBA layout packed\n"
               " * the keysplit pointer into the four ADSR bytes; under PORTABLE that\n"
               " * pointer lives in the separate `keySplitTable` field instead.\n"
               " *\n"
               " * `voice_keysplit`/`voice_keysplit_all` point at another voicegroup, so\n"
               " * every group is declared before any is defined.\n"
               " */")
    for name in groups:
        out.append("extern const struct ToneData %s[];" % name)
    for name, voices in groups.items():
        out.append("")
        out.append("const struct ToneData %s[] = {" % name)
        for macro, args in voices:
            if macro == "voice_keysplit":
                sub, table = args[0], args[1]
                out.append("    { 0x40, 0, 0, 0, (struct WaveData *)%s, "
                           "0, 0, 0, 0, (void *)%s }," % (sub, table))
            elif macro == "voice_keysplit_all":
                out.append("    { 0x80, 0, 0, 0, (struct WaveData *)%s, "
                           "0, 0, 0, 0, NULL }," % args[0])
            else:
                out.append(voice_initializer(macro, args))
        out.append("};")


def check_voice_references(groups):
    """Report voice arguments that are not samples, voicegroups or keysplits.

    A typo here would silently become a link error, or worse a wrong pointer,
    so it is worth a loud report rather than trusting the parse.
    """
    bad = []
    for name, voices in groups.items():
        for macro, args in voices:
            if macro in ("voice_keysplit", "voice_keysplit_all"):
                payload = args[:1] + args[1:2] if macro == "voice_keysplit" else args[:1]
            elif macro in VOICE_MACROS and VOICE_MACROS[macro][0] in ("pcm", "wave"):
                payload = args[2:3]
            else:
                payload = []
            for arg in payload:
                if not re.fullmatch(r"[A-Za-z_]\w*", arg):
                    continue
                if not arg.startswith(SAMPLE_PREFIXES):
                    bad.append((name, macro, arg))
    for entry in bad[:20]:
        warn("voice argument %s in %s (%s) is not a known symbol" % entry[::-1])
    return len(bad)


def parse_keysplit_tables(text):
    """OrderedDict(name -> (offset, [bytes])).

    `.set Name, . - N` puts the label N bytes BEFORE the body so that
    `table[midiKey]` lands on the right voice for tables that only cover part
    of the range.
    """
    tables = OrderedDict()
    current = None
    for raw in text.splitlines():
        line = strip_comment(raw).strip()
        if not line:
            continue
        setter = re.match(r"^\.set\s+(\w+)\s*,\s*\.\s*-\s*(\d+)", line)
        if setter:
            current = setter.group(1)
            tables[current] = (int(setter.group(2)), [])
            continue
        if current is not None:
            byte = re.match(r"^\.byte\s+(\S+)", line)
            if byte:
                tables[current][1].append(int(byte.group(1), 0))
    return tables


def emit_keysplit_tables(tables, out):
    out.append("")
    out.append("/*\n"
               " * Keysplit tables, padded to the full 128-key range and exposed as a\n"
               " * pointer into the pad so `table[midiKey]` reproduces the pret mapping.\n"
               " */")
    for name, (offset, body) in tables.items():
        if offset + len(body) > 128:
            warn("%s overflows the 128-key range (%d + %d)" % (name, offset, len(body)))
        padded = ([0] * offset + body + [0] * 128)[:128]
        out.append("")
        out.append("static const u8 %s_Body[128] = {" % name)
        for start in range(0, 128, 16):
            out.append("    %s," % ", ".join("0x%02X" % b
                                             for b in padded[start:start + 16]))
        out.append("};")
        out.append("const u8 *const %s = &%s_Body[%d];" % (name, name, offset))


def parse_incbin_labels(text):
    """[(label, path)] for every `Label::` followed by `.incbin "path"`."""
    entries = []
    pending = None
    for raw in text.splitlines():
        line = strip_comment(raw).strip()
        label = re.match(r"^(\w+)::\s*$", line)
        if label:
            pending = label.group(1)
            continue
        inc = re.match(r'^\.incbin\s+"([^"]+)"', line)
        if inc and pending:
            entries.append((pending, inc.group(1)))
            pending = None
    return entries


def emit_incbin_entries(entries, decls, defs, comment=None, align="ALIGNED(4) "):
    """Emit `extern const u8 NAME[];` declarations and the INCBIN definitions.

    Split into two lists because the assets are shared: voice_data.h is included
    by both sound_data.c and song_data.h, so a definition in the header would be
    emitted twice. The declarations go in the header, the INCBINs in
    sound_data.c -- the same shape src/data/layouts_data.h uses.
    """
    if comment:
        defs.append("")
        defs.append(comment)
    for label, path in entries:
        decls.append("%sconst u8 %s[];" % (align, label))
        defs.append("")
        defs.append("%sconst u8 %s[] = INCBIN_U8(\"%s\");" % (align, label, path))


# --------------------------------------------------------------------------
# Songs
# --------------------------------------------------------------------------

def parse_midi_cfg(text):
    """`file.mid:  -E -R50 -G159 -V100` -> {song: [options]}."""
    cfg = OrderedDict()
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or ":" not in line:
            continue
        name, opts = line.split(":", 1)
        cfg[name.strip()] = opts.split()
    return cfg


def parse_song_table(text):
    """`song label, ms, me` -> [(label, ms, me)]."""
    songs = []
    for raw in text.splitlines():
        match = re.match(r"^song\s+(\w+)\s*,\s*(\d+)\s*,\s*(\d+)",
                         strip_comment(raw).strip())
        if match:
            songs.append((match.group(1), int(match.group(2)), int(match.group(3))))
    return songs


def assemble_song(name, opts, cache):
    """Run mid2agb for one song, cached; returns the assembler text."""
    if name in cache:
        return cache[name]
    os.makedirs(SONG_CACHE, exist_ok=True)
    src = os.path.join(MIDI_DIR, name + ".mid")
    dst = os.path.join(SONG_CACHE, name + ".s")
    result = subprocess.run([MID2AGB, src, dst] + opts,
                            capture_output=True, text=True)
    if result.returncode != 0:
        raise SystemExit("mid2agb failed for %s: %s" % (name, result.stderr.strip()))
    cache[name] = read(dst)
    return cache[name]


def parse_song_asm(text, base_names):
    """Parse one mid2agb output.

    Returns (names, labels, header_label) where `labels` maps label ->
    [("byte", expr) | ("word", symbol)].
    """
    names = OrderedDict(base_names)
    labels = OrderedDict()
    header_label = None
    current = None

    for raw in text.splitlines():
        line = strip_comment(raw).strip()
        if not line:
            continue
        equ = re.match(r"^\.equ\s+(\w+)\s*,\s*(.+)$", line)
        if equ:
            names[equ.group(1)] = equ.group(2)
            continue
        glob = re.match(r"^\.global\s+(\w+)", line)
        if glob:
            header_label = glob.group(1)
            continue
        label = re.match(r"^(\w+):\s*$", line)
        if label:
            current = label.group(1)
            labels[current] = []
            continue
        data = re.match(r"^\.byte\s+(.+)$", line)
        if data and current:
            labels[current].extend(("byte", part.strip())
                                   for part in data.group(1).split(",") if part.strip())
            continue
        word = re.match(r"^\.word\s+(.+)$", line)
        if word and current:
            labels[current].extend(("word", part.strip())
                                   for part in word.group(1).split(",") if part.strip())

    return names, labels, header_label


def compile_song(name, text, base_names):
    """Serialise one song.

    Returns (names, blobs, fixups, header_bytes, header_words) where `blobs`
    maps label -> bytearray and `fixups` maps label -> [(offset, target_label)].
    """
    names, labels, header_label = parse_song_asm(text, base_names)
    if header_label not in labels:
        raise SystemExit("%s: no header label %r" % (name, header_label))

    header = labels[header_label]
    header_bytes = []
    index = 0
    while index < len(header) and header[index][0] == "byte":
        header_bytes.append(header[index][1])
        index += 1
    header_words = [item[1] for item in header[index:] if item[0] == "word"]
    if len(header_bytes) != 4 or not header_words:
        raise SystemExit("%s: unexpected header shape" % name)

    blobs = OrderedDict()
    fixups = OrderedDict()
    for label, items in labels.items():
        if label == header_label:
            continue
        blob = bytearray()
        ups = []
        index = 0
        while index < len(items):
            kind, value = items[index]
            if kind == "word":
                raise SystemExit("%s: stray .word inside bytecode label %s"
                                 % (name, label))
            opcode = eval_expr(value, names) & 0xFF
            blob.append(opcode)
            if opcode in POINTER_OPCODES:
                # mid2agb always follows the opcode with the target `.word`;
                # the four operand bytes are that target, written little-endian.
                if index + 1 >= len(items) or items[index + 1][0] != "word":
                    raise SystemExit("%s: %s in %s has no .word target"
                                     % (name, POINTER_OPCODES[opcode], label))
                ups.append((len(blob), items[index + 1][1]))
                blob.extend(b"\x00\x00\x00\x00")
                index += 2
                continue
            index += 1
        blobs[label] = blob
        fixups[label] = ups

    return names, blobs, fixups, header_bytes, header_words


def emit_songs(song_list, cfg, base_names, out, out_src):
    """Emit bytecode, headers and the shared pointer index table."""
    cache = {}
    compiled = []
    for name, ms, me in song_list:
        opts = cfg.get(name + ".mid")
        if opts is None:
            raise SystemExit("no midi.cfg entry for %s" % name)
        text = assemble_song(name, opts, cache)
        compiled.append((name,) + compile_song(name, text, base_names))

    # The index table must be complete before any operand is patched, so
    # collect every target first.
    ptr_index = OrderedDict()
    for name, names, blobs, fixups, header_bytes, header_words in compiled:
        for ups in fixups.values():
            for _, target in ups:
                if target in blobs:
                    ptr_index.setdefault(target, len(ptr_index))

    for name, names, blobs, fixups, header_bytes, header_words in compiled:
        for label, ups in fixups.items():
            for offset, target in ups:
                if target not in ptr_index:
                    raise SystemExit("%s: %s targets unknown label %s"
                                     % (name, label, target))
                blobs[label][offset:offset + 4] = ptr_index[target].to_bytes(4, "little")

    out.append("/*\n"
               " * Song bytecode, compiled by tools/gen_sound_data.py from the mid2agb\n"
               " * output of sound/songs/midi/*.mid. Each 4-byte pointer operand\n"
               " * (GOTO/PATT) holds a u32 index into gNativeSongPtrs, which the\n"
               " * interpreter resolves -- the treatment fixes #23/#38 give map and\n"
               " * battle scripts.\n"
               " */")

    # Each *track* is emitted as ONE contiguous array and its labels become
    # offsets into it, because that is what the assembler's layout means: a
    # track's blocks follow one another in memory and the interpreter simply
    # runs off the end of one label into the next. Splitting each label into its
    # own array made that continuation depend on where the linker happened to
    # place them, so a track whose first label was tiny (mus_pallet_1 is just
    # `KEYSH 0`) fell off the end into unrelated data instead of reading its
    # first block.
    for name, names, blobs, fixups, header_bytes, header_words in compiled:
        out.append("")
        out.append("/* %s */" % name)

        # Group the song's labels by the track they belong to, in the order the
        # assembler emits them. A track's blocks are named `<track>_B<n>` (the
        # loop points mid2agb emits), and the track's own entry point is
        # `<track>` with no suffix -- so only a `_B<n>` suffix identifies the
        # track, and stripping at the last underscore unconditionally would fold
        # `mus_pallet_1` into `mus_pallet` and merge every track into one.
        tracks = OrderedDict()
        for label in blobs:
            match = re.match(r"^(.*)_B\d+$", label)
            track = match.group(1) if match else label
            tracks.setdefault(track, []).append(label)

        for track, track_labels in tracks.items():
            offsets = OrderedDict()
            body = bytearray()
            for label in track_labels:
                offsets[label] = len(body)
                body.extend(blobs[label])
            if not body:
                continue

            out.append("")
            out.append("static const u8 %s_Data[] = {" % track)
            for start in range(0, len(body), 12):
                out.append("    %s," % ", ".join("0x%02X" % b
                                                 for b in body[start:start + 12]))
            out.append("};")

            for label in track_labels:
                # A label is a pointer into the track's own buffer, so
                # falling through from one block to the next reads the next
                # block -- exactly like the assembler's layout.
                out.append("#define %s (&%s_Data[%d])"
                           % (label, track, offsets[label]))

        out.append("")
        out.append("/* The track's first label, which is what the song header points at. */")
        for track, track_labels in tracks.items():
            if any(blobs[l] for l in track_labels):
                out.append("#define %s (&%s_Data[0])" % (track, track))

    # A label with no bytes of its own (a loop point emitted right before the
    # block it begins) needs no special handling any more: its offset is simply
    # the start of whatever follows it in the same track buffer, which is what
    # the assembler's address arithmetic resolved to.

    out.append("")
    out.append("/* Every song bytecode label a pointer operand can name. */")
    out.append("const void *const gNativeSongPtrs[] = {")
    for label in ptr_index:
        out.append("    %s," % label)
    out.append("};")

    # Song headers. `struct SongHeader` ends in a variable-length part array,
    # so each header is a local struct with exactly trackCount parts, cast to
    # struct SongHeader * in gSongTable. Field offsets match on both targets
    # (tone at 8, parts at 16).
    out.append("")
    out.append("/*\n"
               " * Song headers: 4 bytes of counters, the voicegroup, then one part\n"
               " * pointer per track. Emitted as an exactly-sized local struct because\n"
               " * `struct SongHeader` declares `part[1]`.\n"
               " */")
    for name, names, blobs, fixups, header_bytes, header_words in compiled:
        tracks = len(header_words) - 1
        counters = [eval_expr(b, names) for b in header_bytes]
        out.append("")
        out.append("static const struct {")
        out.append("    u8 trackCount, blockCount, priority, reverb;")
        out.append("    const struct ToneData *tone;")
        out.append("    const u8 *part[%d];" % tracks)
        out.append("} %s_Header = {" % name)
        out.append("    %d, %d, %d, %d," % tuple(counters))
        out.append("    (const struct ToneData *)%s,"
                   % resolve_symbol(header_words[0], names))
        for part in header_words[1:]:
            out.append("    %s," % part)
        out.append("};")

    return ptr_index, compiled


# --------------------------------------------------------------------------
# Driver
# --------------------------------------------------------------------------

def parse_cry_tables(text):
    """(forward, reverse) lists of sample labels."""
    forward, reverse = [], []
    target = None
    for raw in text.splitlines():
        line = strip_comment(raw).strip()
        if re.match(r"^gCryTable::", line):
            target = forward
            continue
        if re.match(r"^gCryTable_Reverse::", line):
            target = reverse
            continue
        if target is None:
            continue
        match = re.match(r"^(cry|_cry|cry_reverse)\s+(\w+)", line)
        if match and match.group(1) == "cry":
            target.append(match.group(2))
        elif match and match.group(1) == "cry_reverse":
            target.append(match.group(2))
    return forward, reverse


def main():
    base_names = parse_equ_table(read(MPLAYDEF))

    groups = parse_voice_groups(read(VOICE_GROUPS))
    keysplits = parse_keysplit_tables(read(KEYSPLIT_TABLES))
    waves = parse_incbin_labels(read(PROG_WAVES))
    samples = parse_incbin_labels(read(DIRECT_SAMPLES))
    bad_refs = check_voice_references(groups)

    # voice_data.h carries declarations plus the voicegroups/keysplits; the
    # sample and wave blobs are INCBINs and therefore go in sound_data.c, which
    # is the translation unit that must see them. Both files include the header,
    # so a definition there would be emitted twice.
    asset_defs = [
        "/* Generated by tools/gen_sound_data.py. Do not edit. */",
        "",
        "#include \"global.h\"",
        "#include \"gba/m4a_internal.h\"",
        "#include \"data/sound/voice_data.h\"",
    ]
    voices_out = [
        "/* Generated by tools/gen_sound_data.py. Do not edit. */",
        "",
        "#ifndef GUARD_DATA_SOUND_VOICE_DATA_H",
        "#define GUARD_DATA_SOUND_VOICE_DATA_H",
        "",
        "#include \"global.h\"",
        "#include \"gba/m4a_internal.h\"",
        "",
    ]
    voices_out.append("/* Programmable wave RAM data (16 bytes each), loaded by CgbSound. */")
    emit_incbin_entries(waves, voices_out, asset_defs)
    voices_out.append("")
    voices_out.append("/*\n"
                      " * Sample blobs from sound/direct_sound_samples. `struct WaveData` is\n"
                      " * all scalars, so its layout matches the host and a blob casts\n"
                      " * directly to it. audio_rules.mk's wav2agb rules build the .bin\n"
                      " * files these reference.\n"
                      " */")
    emit_incbin_entries(samples, voices_out, asset_defs)
    emit_keysplit_tables(keysplits, voices_out)
    emit_voice_groups(groups, voices_out)

    song_list = parse_song_table(read(SONG_TABLE))
    cfg = parse_midi_cfg(read(MIDI_CFG))

    songs_out = [
        "/* Generated by tools/gen_sound_data.py. Do not edit. */",
        "",
        "#ifndef GUARD_DATA_SOUND_SONG_DATA_H",
        "#define GUARD_DATA_SOUND_SONG_DATA_H",
        "",
        "#include \"global.h\"",
        "#include \"gba/m4a_internal.h\"",
        "#include \"data/sound/voice_data.h\"",
        "",
    ]
    ptr_index, compiled = emit_songs(song_list, cfg, base_names, songs_out,
                                     OUT_SOURCE)

    cry_forward, cry_reverse = parse_cry_tables(read(CRY_TABLES))

    source = asset_defs + [
        "",
        "#include \"data/sound/song_data.h\"",
        "#include \"m4a.h\"",
        "",
        "/*",
        " * Track buffers. pret declares these in sound/music_player_table.inc;",
        " * the counts are its NUM_TRACKS_* values.",
        " */",
        "struct MusicPlayerTrack gMPlayTrack_BGM[10];",
        "struct MusicPlayerTrack gMPlayTrack_SE1[3];",
        "struct MusicPlayerTrack gMPlayTrack_SE2[9];",
        "struct MusicPlayerTrack gMPlayTrack_SE3[1];",
        "",
        "/*",
        " * `struct MusicPlayer.unk_8` is a track count and `unk_A` the player's",
        " * index, both plain scalars, so this table is layout-identical on the",
        " * host (the pointers inside are host-width, which is the point).",
        " */",
        "const struct MusicPlayer gMPlayTable[] = {",
        "    { &gMPlayInfo_BGM, gMPlayTrack_BGM, 10, 0 },",
        "    { &gMPlayInfo_SE1, gMPlayTrack_SE1, 3, 1 },",
        "    { &gMPlayInfo_SE2, gMPlayTrack_SE2, 9, 1 },",
        "    { &gMPlayInfo_SE3, gMPlayTrack_SE3, 1, 0 },",
        "};",
        "",
        "const struct Song gSongTable[] = {",
    ]
    for name, _, _ in song_list:
        source.append("    { (const struct SongHeader *)&%s_Header, %d, %d },"
                      % (name, dict((s[0], s[1:]) for s in song_list)[name][0],
                         dict((s[0], s[1:]) for s in song_list)[name][1]))
    source.append("};")

    for table, entries, type_byte in (("gCryTable", cry_forward, 0x20),
                                      ("gCryTable_Reverse", cry_reverse, 0x30)):
        source.append("")
        source.append("/* %s: one ToneData per species, from sound/cry_tables.inc. */"
                      % table)
        source.append("const struct ToneData %s[] = {" % table)
        for sample in entries:
            source.append("    { 0x%02X, 60, 0, 0, (struct WaveData *)%s, "
                          "0xFF, 0, 0xFF, 0 }," % (type_byte, sample))
        source.append("};")

    voices_out.append("")
    voices_out.append("#endif // GUARD_DATA_SOUND_VOICE_DATA_H")

    songs_out.append("")
    songs_out.append("#endif // GUARD_DATA_SOUND_SONG_DATA_H")

    os.makedirs(os.path.dirname(OUT_VOICES), exist_ok=True)
    for path, lines in ((OUT_VOICES, voices_out), (OUT_SONGS, songs_out),
                        (OUT_SOURCE, source)):
        with open(path, "w") as handle:
            handle.write("\n".join(lines) + "\n")

    print("gen_sound_data: %d voicegroups, %d keysplit tables, %d waves, "
          "%d samples, %d songs, %d cry entries (%d forward / %d reverse), "
          "%d pointer-table entries"
          % (len(groups), len(keysplits), len(waves), len(samples),
             len(song_list), len(cry_forward) + len(cry_reverse),
             len(cry_forward), len(cry_reverse), len(ptr_index)))
    if bad_refs:
        warn("%d voice arguments could not be classified" % bad_refs)


if __name__ == "__main__":
    main()
