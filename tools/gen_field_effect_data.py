#!/usr/bin/env python3
"""Compile data/field_effect_scripts.s into native C.

The field-effect scripts are `.byte`/`.4byte` bytecode interpreted by
src/field_effect.c. Data is emitted byte-for-byte (no translation of opcodes or
operand widths), so the interpreter needs no changes beyond resolving addresses.

Two kinds of 4-byte operand exist, and they are read by different paths:

* ``loadfadedpal``/``loadpal`` take a ``struct SpritePalette *`` and
  ``callnative`` takes a function pointer. On the host these are 64-bit
  pointers, so ``FieldEffectScript_ReadWord`` cannot return one through a u32;
  each is stored as a u32 *index* into the aligned ``gNativeFieldEffectPtrs[]``
  table and resolved there (same treatment as gNativeScriptPtrs /
  gNativeBattlePtrs).
* ``loadtiles`` takes a ``struct SpriteSheet *`` (declared in
  include/sprite.h).  No emitted field-effect script uses it, so an address
  reaching that reader is a hard error rather than something to guess at.

Whether a `callnative` target is emitted at all is decided by scanning the
*linked* sources: the generator reads ENGINE_SRCS/PREPROC_SRCS from
Makefile.native and only symbolises a native that one of those files defines.
Everything else is emitted as a NULL script (the effect simply does not start),
which is the same behaviour the all-NULL stub table had. A hand-maintained list
of "ported" natives would go stale and turn a missing symbol into a link error
or a crash.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPT_SOURCE = os.path.join(ROOT, "data", "field_effect_scripts.s")
MACRO_SOURCE = os.path.join(ROOT, "asm", "macros", "field_effect_script.inc")
MAKEFILE = os.path.join(ROOT, "Makefile.native")
CONSTANTS = os.path.join(ROOT, "include", "constants", "field_effects.h")
OUT_HEADER = os.path.join(ROOT, "src", "data", "field_effects", "scripts_data.h")
OUT_TABLE = os.path.join(ROOT, "src", "data", "field_effects", "ptr_table.c")

# asm/macros/field_effect_script.inc: opcode byte -> operand width, and whether
# the trailing operand is an address. `end` takes no operand.
OPCODES = {
    "loadtiles": (0, 4, True),
    "loadfadedpal": (1, 4, True),
    "loadpal": (2, 4, True),
    "callnative": (3, 4, True),
    "end": (4, 0, False),
    "loadgfx_callnative": (5, 12, True),
    "loadtiles_callnative": (6, 8, True),
    "loadfadedpal_callnative": (7, 8, True),
}


def parse_macros(path):
    """Macro name -> list of parameter names, from the `.macro` definitions."""
    macros = {}
    with open(path) as handle:
        for line in handle:
            match = re.match(r"\s*\.macro\s+(\w+)\s*(.*)", line)
            if match:
                params = [p.strip() for p in match.group(2).split(",") if p.strip()]
                macros[match.group(1)] = params
    return macros


def parse_constants(path):
    """FLDEFF_* -> integer, evaluating simple arithmetic defines.

    FLDEFF_COUNT is `(FLDEFF_PHOTO_FLASH + 1)`, so each define is folded with
    the ones already seen rather than being read as a bare literal.
    """
    constants = {}
    pattern = re.compile(r"\s*#define\s+(FLDEFF_\w+)\s+(.+?)\s*(?://.*)?$")
    for line in open(path):
        match = pattern.match(line)
        if not match:
            continue
        name, expression = match.group(1), match.group(2)
        try:
            value = eval(expression, {"__builtins__": {}}, dict(constants))
        except (NameError, SyntaxError):
            continue  # references something we do not track (e.g. another header)
        if isinstance(value, int):
            constants[name] = value
    return constants


def linked_sources():
    """Source files actually compiled into the binary, from Makefile.native.

    Only these (transitively) can supply a `callnative` target; resolving a
    symbol that is never linked would fail at link time, so an unlinked native
    must stay NULL.
    """
    text = open(MAKEFILE).read()
    # Strip the `include ...mk` directives: spritesheet_rules.mk alone defines
    # hundreds of `%.4bpp` targets that look nothing like source files.
    text = re.sub(r"^include .*$", "", text, flags=re.M)
    return sorted(set(re.findall(r"(src/[\w/]+\.c)", text)))


def include_closure(sources):
    """Every header/source textually reachable from the linked sources.

    Palettes and templates are defined in headers (e.g.
    src/data/field_effects/field_effect_objects.h) that a linked .c includes,
    so a plain scan of the .c files misses them.
    """
    search = [os.path.join(ROOT, "include"), os.path.join(ROOT, "src"), ROOT]
    seen, closure = set(), []
    pending = [os.path.join(ROOT, name) for name in sources]
    while pending:
        path = pending.pop()
        path = os.path.normpath(path)
        if path in seen or not os.path.isfile(path):
            continue
        seen.add(path)
        try:
            text = open(path, errors="replace").read()
        except OSError:
            continue
        closure.append((path, text))
        for target in re.findall(r'^\s*#\s*include\s+"([^"]+)"', text, re.M):
            for root in [os.path.dirname(path)] + search:
                candidate = os.path.join(root, target)
                if os.path.isfile(candidate) and candidate not in seen:
                    pending.append(candidate)
                    break
    return closure


def resolve_symbols(sources, candidates):
    """Which candidate symbols the linked translation units actually define.

    Only true *definitions* count. A header declaring `extern u32 FldEff_X(void)`
    says nothing about whether the .c that defines it is linked, and treating a
    declaration as a definition is exactly the mistake that turns into an
    "Undefined symbols" link error.

    The stub files are read but their definitions are ignored: a `callnative`
    target only the stubs define is a no-op, and resolving to it would silently
    disable the effect instead of leaving it NULL.
    """
    stubs = {os.path.join(ROOT, n) for n in sources if n.startswith("src/platform/")}
    resolved = set()
    for path, text in include_closure(sources):
        if path in stubs:
            continue
        for name in candidates:
            if name in resolved:
                continue
            escaped = re.escape(name)
            # Function definition: the body brace must be present, so a
            # `...);` declaration in a header cannot match.
            if re.search(
                r"^(?!\s*extern\b)[^\n;{}]*\b" + escaped + r"\s*\([^;{}]*\)\s*\{",
                text,
                re.M,
            ):
                resolved.add(name)
                continue
            # Object definition: initialised and not extern/typedef.
            if re.search(
                r"^(?!\s*(?:extern|typedef)\b)[^\n;{}]*\b" + escaped
                + r"\s*(?:\[[^\]]*\])?\s*=",
                text,
                re.M,
            ):
                resolved.add(name)
    return resolved


def parse_scripts(path, macros, constants):
    """(index, label, [tokens]) per entry of gFieldEffectScriptPointers."""
    body = open(path).read()
    # The pointer list is the run of `.4byte label @ comment` lines.
    table, start = [], body.index("gFieldEffectScriptPointers::")
    for line in body[start:].splitlines():
        match = re.match(r"\s*\.4byte\s+(\w+)\s*@\s*(\w+)", line)
        if not match:
            if table:
                break
            continue
        table.append((constants[match.group(2)], match.group(1), match.group(2)))
    if not table:
        raise SystemExit("no gFieldEffectScriptPointers entries parsed")

    # Script bodies: `label::` followed by macro invocations until the next label.
    scripts = {}
    current = None
    for raw in body.splitlines():
        line = raw.split("@")[0].strip()
        if not line:
            continue
        match = re.match(r"^(gFldEffScript_\w+)::", line)
        if match:
            current = match.group(1)
            scripts[current] = []
            continue
        if current is None:
            continue
        tokens = line.split()
        if tokens and tokens[0] in macros:
            scripts[current].append(tokens)

    emitted = []
    for index, label, constant in table:
        if label not in scripts:
            raise SystemExit(f"{constant}: no body for {label}")
        emitted.append((index, label, constant, scripts[label]))
    return emitted, constants["FLDEFF_COUNT"]


class Emitter:
    def __init__(self, defined):
        self.defined = defined
        self.ptrs = [None]        # index 0 is reserved; 0 means "no pointer"
        self.ptr_index = {}
        self.unported = set()
        self.symbol_kinds = {}    # symbol -> "function" | "palette" | "tiles"

    def pointer(self, symbol):
        """Deduplicated index into gNativeFieldEffectPtrs for one symbol."""
        if symbol not in self.ptr_index:
            self.ptr_index[symbol] = len(self.ptrs)
            self.ptrs.append(symbol)
        return self.ptr_index[symbol]

    def script_bytes(self, label, tokens):
        """Bytecode for one script, or None if it needs an unlinked native."""
        out = bytearray()
        for token in tokens:
            op = token[0]
            opcode, width, is_address = OPCODES[op]
            args = [a.strip() for a in " ".join(token[1:]).split(",") if a.strip()]
            if len(args) != width // 4 and width:
                raise SystemExit(f"{label}: {op} got {len(args)} operands")
            out.append(opcode)
            if not width:
                continue
            # The operand kind is fixed by the opcode, so the C declaration the
            # table needs is known without inspecting the definition.
            if op == "loadtiles":
                # struct SpriteSheet *; no emitted script uses it.
                raise SystemExit(f"{label}: loadtiles has no PORTABLE operand path")
            kind = "palette" if "pal" in op else "function"
            for symbol in args:
                if symbol not in self.defined:
                    self.unported.add(symbol)
                    return None
                self.symbol_kinds.setdefault(symbol, kind)
                out += self.pointer(symbol).to_bytes(4, "little")
        return bytes(out)


def main():
    macros = parse_macros(MACRO_SOURCE)
    constants = parse_constants(CONSTANTS)
    sources = linked_sources()
    entries, count = parse_scripts(SCRIPT_SOURCE, macros, constants)

    # Every symbol the scripts reference, so the scan only has to resolve those.
    candidates = {
        symbol
        for _, _, _, tokens in entries
        for token in tokens
        for symbol in " ".join(token[1:]).replace(",", " ").split()
    }
    defined = resolve_symbols(sources, candidates)

    emitter = Emitter(defined)
    bodies, resolved = [], [None] * count
    for index, label, constant, tokens in entries:
        data = emitter.script_bytes(label, tokens)
        if data is None:
            continue
        # Index 0 is a real entry, so every non-NULL script gets a nonzero index
        # and `0` unambiguously means "not ported" in the pointer table.
        resolved[index] = len(bodies) + 1
        bodies.append((label, data))

    with open(OUT_HEADER, "w") as handle:
        handle.write(
            "/* Generated by tools/gen_field_effect_data.py. Do not edit. */\n"
            "/* Field-effect bytecode from data/field_effect_scripts.s. Opcodes and\n"
            " * operand widths are pret's; address operands hold indices into\n"
            " * gNativeFieldEffectPtrs (see src/data/field_effects/ptr_table.c),\n"
            " * resolved by FieldEffectScript_ReadWord under PORTABLE. */\n"
            "/* Each script is the byte sequence of exactly one FLDEFF_* id, so a\n"
            " * script that is not ported simply has no array here. */\n\n"
            "/* Scripts are 4-byte aligned so the interpreter's operand reads are\n"
            " * aligned; gbagfx-free bytecode, so alignment is stated explicitly. */\n\n"
        )
        for label, data in bodies:
            handle.write(
                f"__attribute__((aligned(4)))\nconst u8 {label}[] = {{\n"
            )
            for i in range(0, len(data), 12):
                row = ", ".join(f"0x{b:02X}" for b in data[i:i + 12])
                handle.write(f"    {row},\n")
            handle.write("};\n\n")

    with open(OUT_TABLE, "w") as handle:
        handle.write(
            "/* Generated by tools/gen_field_effect_data.py. Do not edit. */\n\n"
            '#include "global.h"\n'
            '#include "sprite.h"\n'
            '#include "constants/field_effects.h"\n'
            '#include "data/field_effects/scripts_data.h"\n\n'
            "/* The table takes the address of each referenced symbol, so each needs\n"
            " * a declaration here. A callnative target is a function returning a\n"
            " * field-effect id; a palette or sheet operand is an object. */\n"
        )
        for symbol in sorted(emitter.symbol_kinds):
            kind = emitter.symbol_kinds[symbol]
            if kind == "function":
                handle.write(f"extern u32 {symbol}(void);\n")
        handle.write("\n")
        for symbol in sorted(emitter.symbol_kinds):
            kind = emitter.symbol_kinds[symbol]
            if kind == "palette":
                handle.write(f"extern const struct SpritePalette {symbol};\n")
            elif kind == "tiles":
                handle.write(f"extern const struct SpriteSheet {symbol};\n")
            elif kind == "object":
                handle.write(f"extern const void {symbol};\n")
        handle.write("\n")
        handle.write(
            "/* Address operands in the bytecode are u32 indices into this table\n"
            " * (fix #23 style): a 32-bit script operand cannot hold a 64-bit host\n"
            " * pointer. Index 0 is reserved and resolves to NULL so that a\n"
            " * misclassified operand cannot alias a real symbol. */\n"
            "const void *const gNativeFieldEffectPtrs[] = {\n"
            "    NULL,\n"
        )
        for symbol in emitter.ptrs[1:]:
            handle.write(f"    &{symbol},\n")
        handle.write("};\n\n")
        handle.write(
            "/* One entry per FLDEFF_* id; NULL means the effect is not ported.\n"
            " * This is the real definition: the all-NULL stub that used to live in\n"
            " * src/platform/overworld_stubs.c is deleted (fix #8). */\n"
            "const u8 *const gFieldEffectScriptPointers[FLDEFF_COUNT] = {\n"
        )
        for index, label, constant, _ in sorted(entries, key=lambda e: e[0]):
            slot = resolved[index]
            if slot is None:
                handle.write(f"    [{constant}] = NULL,\n")
            else:
                handle.write(f"    [{constant}] = {bodies[slot - 1][0]},\n")
        handle.write("};\n")

    print(
        f"field effects: {len(bodies)} scripts, {len(emitter.ptrs) - 1} pointers "
        f"-> src/data/field_effects/scripts_data.h"
    )
    if emitter.unported:
        names = ", ".join(sorted(emitter.unported))
        print(f"  not ported (left NULL): {names}", file=sys.stderr)


if __name__ == "__main__":
    main()
