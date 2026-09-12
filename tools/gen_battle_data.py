#!/usr/bin/env python3
"""Compile pret's battle script assembly into native 64-bit C data.

The GBA battle engine dispatches through hundreds of script opcodes whose
operands live in ``data/battle_scripts_1.s``, ``data/battle_scripts_2.s``,
``data/battle_anim_scripts.s`` and ``data/battle_ai_scripts.s``. Those files
cannot be assembled for the host: a ``.4byte`` pointer operand is a 32-bit
little-endian address in the GBA build, and the native code reads it back with
``T1_READ_PTR``/``T2_READ_PTR``, which truncate 64-bit host pointers.

This tool emits the scripts with pret's *exact* byte layout, but every 4-byte
operand slot classified as a pointer holds a u32 INDEX into an aligned host
pointer table instead of an address. ``T1_READ_PTR``/``T2_READ_PTR`` resolve
that index at runtime (see src/data/battle_ptr_table.h). Numeric 4-byte slots
are emitted verbatim and stay readable by ``T1_READ_32``/``T2_READ_32``.

Pointer-vs-number classification is NOT hand-maintained: it is derived from the
C readers themselves (``src/battle_script_commands.c``, ``src/battle_anim.c``,
``src/battle_ai_script_commands.c``) so the generated data cannot drift from
the engine that consumes it.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))

from gen_map_data import load_defines  # noqa: E402  (shared constant resolver)


# --------------------------------------------------------------------------
# Source sets
# --------------------------------------------------------------------------

# name -> (assembly paths, C reader path, dispatch table name, macro include,
#          script pointer variable names)
SCRIPT_SETS = {
    "battle": (
        ["data/battle_scripts_1.s", "data/battle_scripts_2.s"],
        "src/battle_script_commands.c",
        "gBattleScriptingCommandsTable",
        "asm/macros/battle_script.inc",
        ["gBattlescriptCurrInstr"],
        False,  # battle handlers read at fixed command-relative offsets
    ),
    "anim": (
        ["data/battle_anim_scripts.s"],
        "src/battle_anim.c",
        "sScriptCmdTable",
        "asm/macros/battle_anim_script.inc",
        ["sBattleAnimScriptPtr"],
        True,   # anim handlers advance the cursor before reading
    ),
    "ai": (
        ["data/battle_ai_scripts.s"],
        "src/battle_ai_script_commands.c",
        "sBattleAICmdTable",
        "asm/macros/battle_ai_script.inc",
        ["sAIScriptPtr"],
        False,  # AI handlers index the script pointer; offsets are literals
    ),
}


# --------------------------------------------------------------------------
# Macro parsing and layout
# --------------------------------------------------------------------------

DIRECTIVE_SIZES = {".byte": 1, ".2byte": 2, ".4byte": 4}


class MacroError(Exception):
    pass


class Macro:
    """A single ``.macro`` definition."""

    def __init__(self, name, params, body):
        self.name = name
        self.params = [p.split(":")[0].split("=")[0] for p in params]
        # ``param=default`` supplies an argument when the invocation omits it
        # (`playanimation_var ... , arg=NULL`).
        self.defaults = {
            p.split(":")[0].split("=")[0]: p.split("=", 1)[1]
            for p in params
            if "=" in p
        }
        # A ``:vararg`` parameter absorbs every remaining argument as one
        # comma-separated list. The anim macros rely on this:
        # `createsprite template, ANIM_ATTACKER, 2, 0, 0, ANIM_TARGET, 2` has
        # three fixed parameters and four varargs. Treating the vararg as an
        # ordinary parameter silently drops all but its first argument and
        # desyncs the whole script stream.
        self.vararg_index = next(
            (
                i
                for i, p in enumerate(params)
                if p.split(":")[1:2] == ["vararg"]
            ),
            None,
        )
        self.body = body  # list of stripped source lines


def load_macros(path):
    """Parse every ``.macro``/``.endm`` block in an assembler include."""
    text = open(os.path.join(ROOT, path)).read()
    macros = {}
    for match in re.finditer(
        r"^[ \t]*\.macro\s+(\w+)([^\n]*)\n(.*?)\n[ \t]*\.endm", text, re.S | re.M
    ):
        # Parameter list is comma separated; each entry may carry a type
        # (`ptr:req`) and/or a default (`arg=NULL`). Split on commas first, then
        # take the name, so a default containing a comma cannot confuse arity.
        spec = match.group(2).strip()
        params = [entry.strip() for entry in spec.split(",")] if spec else []
        body = [
            line.strip()
            for line in match.group(3).splitlines()
            if line.strip() and not line.strip().startswith("@")
        ]
        macros[match.group(1)] = Macro(match.group(1), params, body)
    return macros


def macro_opcode(macro, macros, depth=0):
    """The opcode byte a macro emits itself, or None for pure alias macros.

    Deliberately does NOT follow nested invocations: an alias such as
    ``sethword`` expands to two ``setbyte`` commands, and each of those is a
    separate command with its own operand numbering. Returning the nested
    opcode here would apply ``setbyte``'s slot layout to the whole alias.
    """
    for line in macro.body:
        match = re.match(r"\.byte\s+(0x[0-9a-fA-F]+|\d+)\s*$", line)
        if match:
            return int(match.group(1), 0)
    return None


def expand_layout(macro, macros, depth=0, opcode=None):
    """Byte layout of a macro body as ``(offset, size, operand, opcode)``.

    ``opcode`` identifies the command instance that owns the slot, so nested
    macro invocations keep their own operand numbering: ``sethword`` expands to
    two ``setbyte`` commands, and both carry a pointer at *relative* offset 1.
    Nested invocations are expanded so alias macros (``copybyte`` ->
    ``copyarray``) report the slots of the macro they delegate to, with operand
    expressions written in terms of the *outer* macro's parameters.
    """
    if depth > 12:
        raise MacroError(f"macro recursion at {macro.name}")
    own_opcode = opcode if opcode is not None else macro_opcode(macro, macros)
    slots = []
    offset = 0
    for line in macro.body:
        tokens = line.split(None, 1)
        op = tokens[0]
        operand = tokens[1] if len(tokens) > 1 else ""
        if op in DIRECTIVE_SIZES:
            size = DIRECTIVE_SIZES[op]
            slots.append((offset, size, operand, own_opcode))
            offset += size
        elif op.startswith("."):
            continue  # .if/.else/.endif contribute no fixed bytes here
        else:
            inner = macros.get(op)
            if inner is None:
                continue
            args = [a.strip() for a in operand.split(",")] if operand else []
            substitution = {
                param: (args[i] if i < len(args) else "")
                for i, param in enumerate(inner.params)
            }
            inner_slots = expand_layout(inner, macros, depth + 1)
            for (inner_offset, size, inner_operand, inner_opcode) in inner_slots:
                rewritten = substitute(inner_operand, substitution)
                # Keep the inner offset: it is relative to the nested command
                # instance, which is what the C reader's "+ N" refers to.
                slots.append((inner_offset, size, rewritten, inner_opcode))
            if inner_slots:
                offset += max(o + s for (o, s, _, _) in inner_slots)
    return slots


def substitute(text, substitution):
    """Replace ``\\param`` references using a {param: value} mapping."""
    for param, value in substitution.items():
        text = re.sub(
            r"\\" + re.escape(param) + r"(?![A-Za-z0-9_])",
            lambda _m, v=value: v,
            text,
        )
    return text


def filter_conditionals(body, evaluate):
    """Keep only the active branch of ``.if``/``.elseif``/``.else``/``.endif``.

    Macro bodies may branch on a parameter (`createsprite`'s
    ``.if \\anim_battler == ANIM_TARGET``), and the script files branch on the
    build revision (``.if REVISION >= 0xA``). Emitting both branches produces a
    stream that desynchronises the interpreter; dropping the whole construct
    drops the real bytes.

    ``evaluate`` returns the truth of one condition expression and is only
    called for levels whose parent is active, so an unresolvable condition in a
    skipped branch cannot raise.
    """
    kept = []
    # One entry per open `.if`: (parent_active, any_branch_taken_yet).
    stack = []
    active = True
    for line in body:
        stripped = line.strip()
        if stripped.startswith(".if"):
            parent = active
            cond = evaluate(stripped[3:]) if parent else False
            stack.append([parent, cond])
            active = parent and cond
            continue
        if stripped.startswith(".elseif"):
            if not stack:
                raise MacroError(f".elseif without .if: {line!r}")
            level = stack[-1]
            cond = evaluate(stripped[len(".elseif"):]) if level[0] and not level[1] else False
            level[1] = level[1] or cond
            active = level[0] and cond
            continue
        if stripped.startswith(".else"):
            if not stack:
                raise MacroError(f".else without .if: {line!r}")
            level = stack[-1]
            active = level[0] and not level[1]
            level[1] = True
            continue
        if stripped.startswith(".endif"):
            if not stack:
                raise MacroError(f".endif without .if: {line!r}")
            stack.pop()
            active = stack[-1][0] if stack else True
            continue
        if active:
            kept.append(line)
    if stack:
        raise MacroError("unterminated .if")
    return kept


# --------------------------------------------------------------------------
# Pointer/number classification, derived from the C readers
# --------------------------------------------------------------------------

# Pointer operands are those read with T*_READ_PTR, or read with T*_READ_32 and
# cast straight to a pointer:
#   taskFunc = (TaskFunc)T2_READ_32(sBattleAnimScriptPtr);              typedef
#   template = (const struct SpriteTemplate *)(T2_READ_32(...));         struct *
# (see _is_pointer_cast).
#
# JumpIfMoveFailed(adder, move) reads the command's pointer slot at offset 1 and
# skips `adder` bytes when the move did not fail. Both commands that use it
# (accuracycheck, jumpifaffectedbyprotect) would otherwise classify as UNKNOWN.
JUMP_IF_MOVE_FAILED_RE = re.compile(r"JumpIfMoveFailed\(\s*(\d+)")

# `<handler> , // 0xNN` rows of a command dispatch table.
TABLE_ROW_RE = re.compile(r"(\w+),\s*//\s*(?:0x)?([0-9a-fA-F]+)\s*$", re.M)

# Slots whose C reader cannot be detected mechanically. Two distinct reasons:
#
# * The operand is a real pointer that the engine skips without dereferencing
#   (`copyfoestats`: "Has an unused jump ptr (possibly for a failed attempt)
#   parameter"; anim `jumpifcontest` advances past it unconditionally). These
#   are declared PTR so the table keeps a resolvable entry.
# * The value is a plain number and must stay a literal.
#
# Script set -> {opcode: {offset: 'PTR'|'NUM'}}.
KNOWN_SLOT_OVERRIDES = {
    "battle": {
        0xBD: {1: "PTR"},  # copyfoestats, unused jump pointer
    },
    "anim": {
        0x24: {1: "PTR"},  # jumpifcontest, never-dereferenced pointer operand
    },
    "ai": {
        # `if_random_equal`/`if_random_not_equal` (0x02/0x03) are marked
        # "@ unused" in the macro file: the macro emits the pointer at offset 1
        # while Cmd_if_random_equal reads it at offset 2, so the two disagree by
        # one byte. Nothing invokes them, but the layout must still be declared
        # consistently with what the macro actually emits.
        0x02: {1: "PTR"},
        0x03: {1: "PTR"},
    },
}


def command_body(text, name):
    """Extract the body of ``static void <name>(void) { ... }``."""
    match = re.search(
        r"^static void " + re.escape(name) + r"\(void\)[^\n{]*\n\{", text, re.M
    )
    if not match:
        return None
    start = match.end()
    # Walk braces so a nested block's closing brace does not end the body.
    depth = 1
    index = start
    while index < len(text) and depth:
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
        index += 1
    return text[start : index - 1]


def derive_readers_from_body(body, pointer_var, track_cursor):
    """({ptr offsets}, {num offsets}) for one command body.

    Two operand-spelling dialects exist, and the offset arithmetic differs:

    * Battle commands read at a fixed command-relative offset and only advance
      the pointer to finish the command:

          value = T1_READ_32(gBattlescriptCurrInstr + 6);
          gBattlescriptCurrInstr += 14;

      Here the literal ``+ N`` IS the byte offset within the command, and the
      trailing ``+=`` must not shift any read.

    * Animation and AI commands advance the cursor first and then read bare:

          sBattleAnimScriptPtr++;
          if (gAnimMoveTurn & 1)
              sBattleAnimScriptPtr += 4;
          sBattleAnimScriptPtr = T2_READ_PTR(sBattleAnimScriptPtr);

      Here the offset is wherever the cursor has reached, so motion must be
      tracked (including both sides of a branch).
    """
    ptr = set()
    num = set()
    snapshots = {}
    previous = ""
    pending_if_cursor = None
    block_stack = []
    cursors = {0}

    def resolve(expression):
        """Offset for a read under the current dialect."""
        expression = expression.strip()
        if expression == pointer_var:
            return sorted(cursors)
        match = re.fullmatch(
            r"([A-Za-z_]\w*)\s*\+\s*(0[xX][0-9a-fA-F]+|\d+)", expression
        )
        if match:
            base, delta = match.group(1), int(match.group(2), 0)
            if base == pointer_var:
                base_set = cursors if track_cursor else {0}
                return sorted(c + delta for c in base_set)
            if base in snapshots:
                return sorted(c + delta for c in snapshots[base])
        return []

    for line in body.splitlines():
        stripped = line.strip()
        if not stripped:
            continue

        # Brace scoping: entering a block records the current cursors; leaving
        # merges the block's exit state back in, so a change made on a
        # conditional path is an additional possibility rather than the only one.
        for char in stripped:
            if char == "{":
                block_stack.append(set(cursors))
            elif char == "}" and block_stack:
                cursors = cursors | block_stack.pop()

        # A braceless `if (...)`/`else` makes the *next* statement conditional.
        conditional = bool(re.match(r"(?:if|else)\b", previous)) and "{" not in previous
        if re.match(r"else\b", stripped) and pending_if_cursor is not None:
            cursors = set(pending_if_cursor)
        elif conditional:
            pending_if_cursor = set(cursors)
        previous = stripped

        snapshot = re.match(
            r"(?:const\s+)?u8\s*\*\s*(\w+)\s*=\s*" + re.escape(pointer_var) + r"\s*;",
            stripped,
        )
        if snapshot:
            snapshots[snapshot.group(1)] = set(cursors)
            continue

        for pattern, kind in (
            (r"T[12]_READ_PTR\(([^)]*)\)", "ptr"),
            (r"T[12]_READ_32\(([^)]*)\)", "cast"),
            (r"T[12]_READ_(?:16|8)\(([^)]*)\)", "num"),
        ):
            for match in re.finditer(pattern, stripped):
                for offset in resolve(match.group(1)):
                    if kind == "ptr":
                        ptr.add(offset)
                    elif kind == "cast":
                        # A `_32` read is a pointer operand only when it is cast
                        # to one; otherwise it is a plain number.
                        (ptr if _is_pointer_cast(stripped, match.start()) else num).add(
                            offset
                        )
        if not track_cursor:
            continue

        advances = len(re.findall(re.escape(pointer_var) + r"\+\+", stripped))
        for _ in range(advances):
            cursors = {c + 1 for c in cursors}
        for match in re.finditer(
            re.escape(pointer_var) + r"\s*\+=\s*(0[xX][0-9a-fA-F]+|\d+)", stripped
        ):
            shifted = {c + int(match.group(1), 0) for c in cursors}
            cursors = cursors | shifted if conditional else shifted
        jump = re.search(
            re.escape(pointer_var) + r"\s*=\s*(\w+)\s*\+\s*(0[xX][0-9a-fA-F]+|\d+)",
            stripped,
        )
        if jump and jump.group(1) in snapshots:
            cursors = {c + int(jump.group(2), 0) for c in snapshots[jump.group(1)]}
    return ptr, num


def _is_pointer_cast(line, read_start):
    """Whether the text immediately before a ``_32`` read is a pointer cast."""
    prefix = line[:read_start].rstrip()
    # Already-resolved form: gNativeBattlePtrs[T2_READ_32(...)]
    if prefix.endswith("gNativeBattlePtrs["):
        return True
    return bool(
        re.search(
            r"\(\s*(?:const\s+)?(?:struct\s+\w+|union\s+\w+)(?:\s*\*+)?\s*\)\s*\(?$",
            prefix,
        )
        or re.search(r"\(\s*\w*Func\s*\)\s*$", prefix)
    )


def derive_opcode_readers(c_path, table_name, overrides, pointer_vars, track_cursor):
    """Map opcode number -> ({ptr offsets}, {num offsets}) for a command table.

    The dispatch table is authoritative for opcode -> handler mapping, which
    matters where a macro's name differs from its handler (`tryfaintmon_spikes`
    encodes 0x19, handled by ``Cmd_tryfaintmon``).
    """
    text = open(os.path.join(ROOT, c_path)).read()
    match = re.search(
        r"\b" + re.escape(table_name) + r"\s*\[\][^={]*=\s*\{(.*?)\n\};", text, re.S
    )
    if not match:
        raise MacroError(f"{c_path}: dispatch table {table_name} not found")
    readers = {}
    for name, index in TABLE_ROW_RE.findall(match.group(1)):
        body = command_body(text, name)
        if body is None:
            raise MacroError(f"{c_path}: body of {name} not found")
        opcode = int(index, 16)
        ptr, num = set(), set()
        for pointer_var in pointer_vars:
            if pointer_var not in body:
                continue
            body_ptr, body_num = derive_readers_from_body(
                body, pointer_var, track_cursor
            )
            ptr |= body_ptr
            num |= body_num
        if JUMP_IF_MOVE_FAILED_RE.search(body):
            ptr.add(1)
        for offset, kind in overrides.get(opcode, {}).items():
            (ptr if kind == "PTR" else num).add(offset)
        readers[opcode] = (ptr, num)
    return readers


def macro_opcode(macro, macros, depth=0):
    """The opcode byte a macro encodes, or None for alias macros."""
    if depth > 12:
        raise MacroError(f"macro recursion at {macro.name}")
    for line in macro.body:
        match = re.match(r"\.byte\s+(0x[0-9a-fA-F]+|\d+)\s*$", line)
        if match:
            return int(match.group(1), 0)
        opcode = line.split()[0]
        if opcode in macros:
            return macro_opcode(macros[opcode], macros, depth + 1)
    return None


def derive_macro_slot_kinds(macros, readers):
    """macro name -> {(opcode, offset): 'PTR'|'NUM'} for every 4-byte slot.

    Nested commands keep their own operand numbering, so a slot is keyed by the
    opcode that owns it rather than by an absolute byte offset.
    """
    kinds = {}
    for name, macro in macros.items():
        slots = {}
        for (offset, size, _operand, opcode) in expand_layout(macro, macros):
            if size != 4:
                continue
            if opcode is None:
                raise MacroError(f"{name}: cannot determine opcode for slot {offset}")
            ptr, num = readers[opcode]
            if offset in ptr:
                slots[(opcode, offset)] = "PTR"
            elif offset in num:
                slots[(opcode, offset)] = "NUM"
            else:
                raise MacroError(
                    f"{name}: 4-byte slot at offset {offset} is neither a pointer "
                    f"nor a number reader in opcode {opcode:#x}"
                )
        kinds[name] = slots
    return kinds


# --------------------------------------------------------------------------
# Expression evaluation
# --------------------------------------------------------------------------

class Symbols:
    """Resolves assembler symbols to native C expressions."""

    def __init__(
        self,
        constants,
        local_labels,
        external_kinds,
        pointer_defines=None,
        function_macros=None,
    ):
        self.constants = constants
        self.local_labels = local_labels  # labels emitted as C arrays
        self.external_kinds = external_kinds  # name -> 'array'|'func'|'var'
        self.pointer_defines = pointer_defines or {}
        self.function_macros = function_macros or {}  # name -> (params, body)

    def evaluate(self, expression):
        """Evaluate a numeric operand expression."""
        expression = expression.strip()
        if not expression:
            raise MacroError("empty operand")
        # Try a plain constant first, then fall back to a full eval.
        if expression in self.constants:
            return self.constants[expression]
        try:
            return int(expression, 0)
        except ValueError:
            pass
        rewritten = self.expand_function_macros(expression)
        rewritten = re.sub(
            r"\b(?:u8|u16|u32|s8|s16|s32|vu8|vu16|vu32)\b", "", rewritten
        )
        rewritten = re.sub(r"\b(0[xX][0-9a-fA-F]+|\d+)[uUlL]+\b", r"\1", rewritten)
        try:
            return int(eval(rewritten, {"__builtins__": {}}, dict(self.constants)))
        except Exception as error:  # noqa: BLE001 - surfaced as a generator error
            raise MacroError(f"cannot evaluate {expression!r}: {error}") from error

    def expand_function_macros(self, expression):
        """Inline function-like header macros, e.g. ``RGB(8, 9, 28)``.

        Animation scripts pass colour literals to ``createsprite``; those are
        ``#define RGB(r, g, b) ((r) | ((g) << 5) | ((b) << 10))`` rather than
        constants, so they must be substituted before evaluation.
        """
        for _ in range(8):
            match = re.search(r"\b([A-Za-z_]\w*)\s*\(", expression)
            if not match or match.group(1) not in self.function_macros:
                break
            name = match.group(1)
            params, body = self.function_macros[name]
            start = match.end()
            depth = 1
            end = start
            while end < len(expression) and depth:
                if expression[end] == "(":
                    depth += 1
                elif expression[end] == ")":
                    depth -= 1
                end += 1
            call_args = _split_top_level(expression[start : end - 1])
            if len(call_args) != len(params):
                break
            substituted = body
            for param, value in zip(params, call_args):
                substituted = re.sub(
                    r"\b" + re.escape(param) + r"\b", value, substituted
                )
            expression = expression[: match.start()] + substituted + expression[end:]
        return expression

    def pointer_expression(self, operand):
        """Resolve a pointer operand to a native C expression."""
        operand = operand.strip()
        if operand in ("NULL", "0"):
            return "NULL"
        match = re.fullmatch(r"([A-Za-z_]\w*)\s*\+\s*(.+)", operand)
        if match:
            base, rest = match.group(1), self.evaluate(match.group(2))
            return self._base_expression(base, rest)
        match = re.fullmatch(r"([A-Za-z_]\w*)", operand)
        if match:
            return self._base_expression(match.group(1), 0)
        raise MacroError(f"unsupported pointer operand {operand!r}")

    def _base_expression(self, name, extra):
        if name in self.local_labels:
            return name if not extra else f"{name} + {extra}"
        if name in self.pointer_defines:
            # A header macro that aliases a pointer expression, e.g.
            #   #define sMOVEEND_STATE  gBattleScripting + 0x14
            # The assembler addend is a BYTE offset, but C pointer arithmetic
            # scales by the pointee size, so `&gBattleScripting + 0x14` would
            # advance 20 structs. Cast to a byte pointer to reproduce the
            # assembler's semantics exactly.
            expression = self.pointer_defines[name]
            match = re.fullmatch(
                r"([A-Za-z_]\w*)\s*\+\s*(0[xX][0-9a-fA-F]+|\d+)", expression
            )
            if match and match.group(1) not in self.local_labels:
                base_name = match.group(1)
                byte_offset = int(match.group(2), 0)
                kind = self.external_kinds.get(base_name)
                if kind == "var":
                    base = f"&{base_name}"
                elif kind in ("array", "func"):
                    base = base_name
                else:
                    raise MacroError(f"unresolved pointer symbol {base_name!r}")
                total = byte_offset + extra
                return f'(void *)((u8 *){base} + {total})'
            base = self.pointer_expression(expression)
            return base if not extra else f"(void *)((u8 *){base} + {extra})"
        if name in self.constants:
            raise MacroError(f"{name!r} is a numeric constant, not a pointer")
        kind = self.external_kinds.get(name)
        if kind == "var":
            base = f"&{name}"
        elif kind in ("array", "func"):
            base = name
        else:
            raise MacroError(f"unresolved pointer symbol {name!r}")
        return base if not extra else f"(void *)((u8 *){base} + {extra})"


def collect_pointer_defines():
    """Header ``#define``s that alias a pointer expression rather than a number.

    pret names battle-script operands such as ``sMOVEEND_STATE`` through
    ``include/constants/battle_script_commands.h``:

        #define sMOVEEND_STATE  gBattleScripting + 0x14

    Those resolve to an address, so they must be expanded rather than evaluated
    as integers.
    """
    defines = {}
    for path in _header_paths():
        for raw in open(path, errors="ignore").read().splitlines():
            body = raw.split("//", 1)[0]
            match = re.match(r"\s*#define\s+([A-Za-z_]\w*)\s+(.+?)\s*$", body)
            if not match:
                continue
            name, expression = match.group(1), match.group(2).strip()
            # Accept `gBattleScripting + 0x14` and `(gBattleCommunication + X)`.
            # Reject numeric-only defines and anything with a comma/operator
            # that would not denote an address.
            candidate = expression.strip().strip("()").strip()
            if not re.fullmatch(r"[A-Za-z_]\w*(\s*\+\s*[A-Za-z0-9_x]+)?", candidate):
                continue
            if re.search(r"\bg[A-Z]\w*|\bs[A-Z]\w*", candidate):
                defines[name] = candidate
    return defines


def collect_pointer_tables():
    """Names declared as arrays of pointers, e.g. ``extern const u8 *const X[]``.

    In pret sources every global label is written with ``::``, so the assembler
    form cannot distinguish a script body from a pointer table. The C headers
    can: a pointer table is declared as an array whose element type is a
    pointer. Tables are emitted as ``const u8 *const NAME[]`` and their entries
    are resolved directly; plain scripts are emitted as ``const u8 NAME[]``.
    """
    tables = set()
    for path in list(_header_paths()) + list(_source_paths()):
        for raw in open(path, errors="ignore").read().splitlines():
            body = raw.split("//", 1)[0]
            match = re.search(
                r"\b([A-Za-z_]\w*)\s*\[[^\]]*\]\s*;", body
            )
            if not match:
                continue
            declaration = body[: match.start(1)]
            # Element type must itself be a pointer: `... * X[]` or `... *const X[]`.
            if "*" in declaration:
                tables.add(match.group(1))
    return tables


def collect_function_macros():
    """Function-like header macros whose body is a constant expression.

    e.g. ``#define RGB(r, g, b) ((r) | ((g) << 5) | ((b) << 10))``.
    """
    macros = {}
    for path in _header_paths():
        for raw in open(path, errors="ignore").read().splitlines():
            body = raw.split("//", 1)[0]
            match = re.match(r"\s*#define\s+(\w+)\(([^)]*)\)\s+(.+?)\s*$", body)
            if not match:
                continue
            params = [p.strip() for p in match.group(2).split(",") if p.strip()]
            expression = match.group(3).strip()
            if not params or "," in expression and ";" in expression:
                continue
            macros[match.group(1)] = (params, expression)
    return macros


def _split_top_level(text):
    """Split on commas that are not nested inside parentheses."""
    parts = []
    depth = 0
    current = ""
    for char in text:
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
        if char == "," and depth == 0:
            parts.append(current.strip())
            current = ""
        else:
            current += char
    if current.strip():
        parts.append(current.strip())
    return parts


def resolve_function_macro_constants(constants, function_macros):
    """Evaluate numeric constants whose definition uses a function-like macro.

    ``#define RGB_BLACK RGB(0, 0, 0)`` is invisible to the plain constant
    resolver, because ``RGB`` is a function-like macro rather than a value.
    """
    probe = Symbols(constants, set(), {}, {}, function_macros)
    for name, expression in list(function_macro_constants().items()):
        if name in constants:
            continue
        try:
            constants[name] = probe.evaluate(expression)
        except MacroError:
            continue
    return constants


def function_macro_constants():
    """Every object-like ``#define`` whose body mentions a call."""
    out = {}
    for path in _header_paths():
        for raw in open(path, errors="ignore").read().splitlines():
            body = raw.split("//", 1)[0]
            match = re.match(r"\s*#define\s+([A-Za-z_]\w*)\s+(.+?)\s*$", body)
            if not match or "(" in match.group(1):
                continue
            out.setdefault(match.group(1), match.group(2))
    return out


def collect_declarations():
    """Minimal ``extern`` declaration for every symbol defined in ``src/*.c``.

    The generated pointer table takes the address of ~1600 engine symbols that
    no shared header declares (`AnimTask_*` animation callbacks, per-sprite
    ``g*SpriteTemplate`` structs, battle data tables). Rather than hand-writing
    declarations, they are derived mechanically from the definitions:

        void AnimTask_ShakeMon(u8 taskId)      -> extern void AnimTask_ShakeMon(u8 taskId);
        const u16 gStatUpStringIds[] = {...}   -> extern const u16 gStatUpStringIds[];
        EWRAM_DATA u8 gNumSafariBalls = 0;     -> extern u8 gNumSafariBalls;
        const struct SpriteTemplate gX = {...} -> extern const struct SpriteTemplate gX;

    A brace/paren-aware scan is required because initializers span lines and a
    function definition ends in ``{`` rather than ``;``.
    """
    declarations = {}
    for path in _source_paths():
        if not path.endswith(".c"):
            continue
        text = _strip_comments(open(path, errors="ignore").read())
        for match in _DECLARATOR_RE.finditer(text):
            qualifiers = match.group(1) or ""
            type_text = match.group(2).strip()
            name = match.group(3)
            tail = match.group(4) or ""
            # Strip storage-class/data-section qualifiers; keep constness.
            extra = " ".join(
                token
                for token in qualifiers.split()
                if token in ("const", "volatile")
            )
            head = f"{extra} {type_text}".strip() if extra else type_text
            # A declarator whose name is wrapped in parentheses
            # (`const u8 (*const X[])[4]`) is captured with the wrapper in the
            # tail rather than before the name, so it cannot be reassembled from
            # the groups. Skip it and let the owning header declare it.
            if tail.lstrip().startswith("(*"):
                continue
            if tail.startswith("("):
                tail = re.sub(r"\s+", " ", tail).strip()
                declarations.setdefault(name, f"extern {head} {name}{tail};")
            elif tail.startswith("["):
                declarations.setdefault(name, f"extern {head} {name}{tail};")
            else:
                declarations.setdefault(name, f"extern {head} {name};")
    return declarations


# Declarator followed by `(` (function) or `[` (array) or an initializer.
_DECLARATOR_RE = re.compile(
    r"^(?:(static|EWRAM_DATA|COMMON_DATA)\s+)?"
    r"((?:const\s+)?(?:struct\s+\w+|union\s+\w+|enum\s+\w+|unsigned\s+\w+|[A-Za-z_]\w*)"
    r"(?:\s*\*+)?)\s+"
    r"([A-Za-z_]\w*)\s*"
    r"(\[[^\]]*\]|\([^;{)]*(?:\)[^;{]*)?)?\s*"
    r"(?=[={;])",
    re.M,
)


def _strip_comments(text):
    """Remove C comments so declaration scanning is not confused by prose."""
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", " ", text)


def needed_declarations(expressions, include_text):
    """Declarations required by the generated pointer table but not visible.

    Only symbols the emitted expressions actually reference are declared, and
    only when the headers the generated file already includes do not declare
    them, so this cannot conflict with an existing prototype.
    """
    available = collect_declarations()
    already = set(re.findall(r"\bextern\b[^;]*?\b([A-Za-z_]\w*)\s*(?:\[|\()", include_text))
    already |= set(re.findall(r"^extern[^;]*?\b([A-Za-z_]\w*)\s*;", include_text, re.M))

    referenced = set()
    for expression in expressions:
        for name in re.findall(r"\b([A-Za-z_]\w*)\b", expression):
            if name in available and name not in already:
                referenced.add(name)
    return [available[name] for name in sorted(referenced)]


def collect_u16_tables():
    """Names declared as arrays of u16 (value tables, not pointer tables)."""
    tables = set()
    for path in list(_header_paths()) + list(_source_paths()):
        for raw in open(path, errors="ignore").read().splitlines():
            body = raw.split("//", 1)[0]
            if re.search(r"\bconst\s+u16\s+([A-Za-z_]\w*)\s*\[", body):
                tables.add(re.search(r"\bconst\s+u16\s+([A-Za-z_]\w*)\s*\[", body).group(1))
    return tables


def collect_external_kinds():
    """Classify every externally visible C symbol as array/func/var."""
    kinds = {}
    for path in list(_header_paths()) + list(_source_paths()):
        text = open(path, errors="ignore").read()
        for match in re.finditer(
            r"^\s*(?:extern\s+)?(?:const\s+|static\s+|EWRAM_DATA\s+|COMMON_DATA\s+)*"
            r"(?:struct\s+\w+|union\s+\w+|enum\s+\w+|unsigned\s+\w+|[A-Za-z_]\w*)"
            r"\s*\**\s*([A-Za-z_]\w*)\s*(\[[^\]]*\]\s*=|\([^;{]*\)\s*[;{]|=|;)",
            text,
            re.M,
        ):
            name, tail = match.group(1), match.group(2)
            if tail.startswith("["):
                kinds.setdefault(name, "array")
            elif tail.startswith("("):
                kinds.setdefault(name, "func")
            else:
                kinds.setdefault(name, "var")
    return kinds


def _header_paths():
    include = os.path.join(ROOT, "include")
    for base, _dirs, files in os.walk(include):
        for name in files:
            if name.endswith(".h"):
                yield os.path.join(base, name)


def _source_paths():
    src = os.path.join(ROOT, "src")
    for base, dirs, files in os.walk(src):
        dirs[:] = [d for d in dirs if d != "platform"]
        for name in files:
            if name.endswith(".c") or name.endswith(".h"):
                yield os.path.join(base, name)


# --------------------------------------------------------------------------
# Script assembly
# --------------------------------------------------------------------------

LABEL_RE = re.compile(r"^([A-Za-z_]\w*)\s*(::?)\s*(?:@.*)?$")


class PointerRegistry:
    """Ordered, deduplicated table of host pointer expressions.

    Shared by every script set so the interpreters resolve an operand index
    identically regardless of which dialect emitted it.
    """

    def __init__(self):
        self.entries = []
        self.indices = {}

    def index_of(self, expression):
        if expression not in self.indices:
            self.indices[expression] = len(self.entries)
            self.entries.append(expression)
        return self.indices[expression]

    def __len__(self):
        return len(self.entries)


class Assembled:
    def __init__(self):
        self.scripts = []  # (name, tokens)
        self.tables = []  # (name, [pointer expressions])


class Assembler:
    """Assembles one script set (battle, anim or ai)."""

    def __init__(self, macros, slot_kinds, symbols, constants, pointer_tables, registry,
                 u16_tables=None):
        self.macros = macros
        self.slot_kinds = slot_kinds
        self.symbols = symbols
        self.constants = constants
        self.pointer_tables = pointer_tables
        self.u16_tables = u16_tables or set()
        # One registry is shared by every script set so a single pointer table
        # (and therefore a single reader macro) serves the battle, animation and
        # AI interpreters alike.
        self.registry = registry
        self.macro_counter = 0
        self.result = Assembled()

    def pointer_index_for(self, expression):
        return self.registry.index_of(expression)

    # -- macro expansion ---------------------------------------------------
    def emit_macro(self, macro_name, args, output):
        """Expand one command instance into output tokens.

        Returns the number of bytes emitted. Nested invocations are expanded
        recursively and each keeps its own operand numbering, so a 4-byte slot
        is classified by ``(opcode, offset)`` rather than by an absolute offset.
        """
        macro = self.macros[macro_name]
        substitution = {}
        for i, param in enumerate(macro.params):
            if i < len(args) and args[i] != "":
                substitution[param] = args[i]
            else:
                substitution[param] = macro.defaults.get(param, "")
        kinds = self.slot_kinds[macro_name]
        own_opcode = macro_opcode(macro, self.macros)
        emitted = 0
        self.macro_counter += 1
        unique = self.macro_counter
        # Labels are resolved before emission because the byte count that
        # references them precedes their definition in the macro body
        # (`createsprite` emits the vararg count, then defines .Lsprite_X_1).
        body = [substitute(line, substitution).replace("\\@", str(unique)) for line in macro.body]
        # Only the taken branch of `.if \\param == X` may be emitted; emitting
        # both desynchronises every following command.
        body = filter_conditionals(body, self.evaluate_expression)
        labels = self.layout_labels(body, macro_name)

        for line in body:
            stripped = line.strip()

            if re.fullmatch(r"\.L\w+:", stripped):
                continue

            tokens = line.split(None, 1)
            op = tokens[0]
            operand = tokens[1] if len(tokens) > 1 else ""

            if op in DIRECTIVE_SIZES:
                size = DIRECTIVE_SIZES[op]
                if self.has_layout_expression(operand):
                    output.append(self.layout_byte_expression(operand, labels))
                    emitted += 1
                    continue
                operands = self.split_operands(operand)
                for value in operands:
                    if size == 4 and kinds.get((own_opcode, emitted)) == "PTR":
                        expression = self.symbols.pointer_expression(value)
                        # The index occupies the operand's full 4-byte slot,
                        # little-endian, so the layout is byte-identical to the
                        # GBA build apart from the value's meaning.
                        self.append_integer(
                            output, self.pointer_index_for(expression), 4
                        )
                    else:
                        self.append_integer(output, self.symbols.evaluate(value), size)
                emitted += size * len(operands)
            elif op.startswith("."):
                continue
            elif op in self.macros:
                inner = self.macros[op]
                nested_args = self.split_args(
                    operand, len(inner.params), inner.vararg_index
                )
                emitted += self.emit_macro(op, nested_args, output)
            else:
                raise MacroError(f"{macro_name}: unknown macro line {line!r}")
        return emitted

    def layout_labels(self, body, macro_name):
        """Byte offset of every ``.L*`` label in a substituted macro body.

        Labels are macro-local; nested invocations only contribute their size,
        so a label's offset is computed as the running total of everything
        emitted before it.
        """
        offsets = {}
        emitted = 0
        for line in body:
            stripped = line.strip()
            if re.fullmatch(r"\.L\w+:", stripped):
                offsets[stripped[:-1]] = emitted
                continue
            tokens = line.split(None, 1)
            op = tokens[0]
            operand = tokens[1] if len(tokens) > 1 else ""
            if op in DIRECTIVE_SIZES:
                if self.has_layout_expression(operand):
                    emitted += 1
                else:
                    emitted += DIRECTIVE_SIZES[op] * len(self.split_operands(operand))
            elif op in self.macros:
                inner = self.macros[op]
                nested_args = self.split_args(
                    operand, len(inner.params), inner.vararg_index
                )
                emitted += self.macro_size(inner, nested_args)
        return offsets

    def macro_size(self, macro, args, depth=0):
        """Total byte count a macro invocation expands to."""
        if depth > 12:
            raise MacroError(f"macro recursion at {macro.name}")
        substitution = {
            param: (
                args[i]
                if i < len(args) and args[i] != ""
                else macro.defaults.get(param, "")
            )
            for i, param in enumerate(macro.params)
        }
        total = 0
        body = [substitute(line, substitution) for line in macro.body]
        body = filter_conditionals(body, self.evaluate_expression)
        for text in body:
            tokens = text.split(None, 1)
            op = tokens[0]
            operand = tokens[1] if len(tokens) > 1 else ""
            if op in DIRECTIVE_SIZES:
                total += (
                    1
                    if self.has_layout_expression(operand)
                    else DIRECTIVE_SIZES[op] * len(self.split_operands(operand))
                )
            elif op in self.macros:
                inner = self.macros[op]
                nested_args = self.split_args(
                    operand, len(inner.params), inner.vararg_index
                )
                total += self.macro_size(inner, nested_args, depth + 1)
        return total

    @staticmethod
    def append_integer(output, value, size):
        """Append a little-endian integer as individual bytes."""
        value &= (1 << (size * 8)) - 1
        for shift in range(0, size * 8, 8):
            output.append((value >> shift) & 0xFF)

    @staticmethod
    def has_layout_expression(operand):
        return bool(re.search(r"\.L\w+", operand))

    def layout_byte_expression(self, operand, labels):
        """Resolve ``(.Lfoo_2 - .Lfoo_1) / N`` computed byte counts.

        ``createsprite``/``createvisualtask``/``createsoundtask`` encode the
        number of varargs as the distance between two macro-local labels.
        """
        match = re.fullmatch(
            r"\(\s*(\.L\w+)\s*-\s*(\.L\w+)\s*\)\s*/\s*(\d+)", operand.strip()
        )
        if not match:
            raise MacroError(f"unsupported computed byte expression {operand!r}")
        first, second, divisor = match.group(1), match.group(2), int(match.group(3))
        if first not in labels or second not in labels:
            raise MacroError(f"unresolved layout label in {operand!r}")
        # `first` names the end label and `second` the start label (the macro
        # writes `(.Lend - .Lstart)`), so the count is end - start. Reversing
        # them yields a negative count that the interpreter uses as a loop
        # bound, walking the anim argument list off the end of the command.
        return ((labels[first] - labels[second]) // divisor) & 0xFF

    @staticmethod
    def split_operands(operand):
        """Split an operand list on commas, ignoring commas inside parens."""
        parts = []
        depth = 0
        current = ""
        for char in operand:
            if char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
            if char == "," and depth == 0:
                parts.append(current.strip())
                current = ""
            else:
                current += char
        if current.strip():
            parts.append(current.strip())
        return parts

    @classmethod
    def split_args(cls, operand, param_count, vararg_index=None):
        """Split a macro invocation's arguments the way GNU as does.

        GNU as separates macro arguments on commas, but also accepts whitespace
        between them. pret relies on both forms:

            setbyte sMOVEEND_STATE, \\case          @ comma separated
            setbyte sSTATCHANGER \\stat | \\stages << 4   @ space separated

        So: split on commas first, and if that yields fewer pieces than the
        macro declares parameters, split on whitespace with the final parameter
        absorbing the remainder.

        A ``:vararg`` parameter absorbs *every* remaining argument as one
        comma-separated list, which is how the animation macros pass sprite and
        task arguments:

            createsprite gBasicHitSplatSpriteTemplate, ANIM_ATTACKER, 2, 0, 0, ANIM_TARGET, 2

        Three fixed parameters plus four varargs. Splitting positionally would
        bind ``argv`` to the first vararg and silently drop the rest, which
        desynchronises the emitted script.
        """
        if vararg_index is not None and param_count:
            parts = cls.split_operands(operand)
            if len(parts) == 1 and vararg_index:
                words = parts[0].split(None, vararg_index)
                if len(words) == vararg_index + 1:
                    parts = words
            if len(parts) > vararg_index:
                return parts[:vararg_index] + [", ".join(parts[vararg_index:])]
        parts = cls.split_operands(operand)
        if len(parts) >= param_count or param_count == 0:
            return parts
        if len(parts) != 1:
            return parts
        words = parts[0].split(None, param_count - 1)
        return words if len(words) == param_count else parts

    # -- top level ---------------------------------------------------------
    def assemble_files(self, paths):
        for rel in paths:
            self.assemble_file(rel)

    def assemble_file(self, rel):
        path = os.path.join(ROOT, rel)
        lines = open(path).read().splitlines()
        current = None
        pending_table = None

        # Conditional assembly (`REVISION >= 0xA`) uses the same semantics as a
        # macro body's `.if`, so both go through one filter.
        kept_lines = filter_conditionals(lines, self.evaluate_condition)

        for raw in kept_lines:
            line = raw.split("@", 1)[0].rstrip() if not raw.strip().startswith("@") else ""
            stripped = line.strip()
            if not stripped:
                continue
            if stripped.startswith("#include") or stripped.startswith(".include"):
                continue
            if stripped.startswith(".section") or stripped.startswith(".align") or stripped.startswith(".set"):
                continue

            label = LABEL_RE.match(stripped)
            if label:
                name = label.group(1)
                if name in self.u16_tables:
                    # A value table (u16 elements), e.g. gMovesWithQuietBGM.
                    pending_table = (name, [])
                    self.result.tables.append(pending_table)
                    current = None
                elif name in self.pointer_tables:
                    pending_table = (name, [])
                    self.result.tables.append(pending_table)
                    current = None
                else:
                    pending_table = None
                    current = (name, [])
                    self.result.scripts.append(current)
                continue

            if pending_table is not None:
                if stripped.startswith(".4byte") or stripped.startswith(".2byte"):
                    table = pending_table[1]
                    for value in self.split_operands(stripped.split(None, 1)[1]):
                        table.append(value)
                    continue
                if stripped.startswith("."):
                    continue

            if stripped.startswith("."):
                # Stray data directives between scripts (rare); keep them raw.
                continue

            if current is None:
                continue
            # The command name ends at the first whitespace; everything after is
            # the argument list (which may itself be comma or whitespace separated).
            parts = stripped.split(None, 1)
            opcode = parts[0]
            arg_text = parts[1] if len(parts) > 1 else ""
            if opcode not in self.macros:
                raise MacroError(f"{rel}: unknown script command {opcode!r}")
            args = self.split_args(
                arg_text,
                len(self.macros[opcode].params),
                self.macros[opcode].vararg_index,
            )
            self.emit_macro(opcode, args, current[1])

    def evaluate_condition(self, expression):
        """Truth of a script-file ``.if`` (e.g. ``REVISION >= 0xA``)."""
        expression = expression.strip()
        try:
            return bool(self.symbols.evaluate(expression))
        except MacroError:
            # Unresolvable conditions (e.g. REVISION) fall back to the
            # non-.A branch, which is what REVISION=0 selects.
            return False

    def evaluate_expression(self, expression):
        """Truth of a macro-body ``.if``, after parameter substitution.

        Macro bodies are substituted before filtering, so the expression is
        already concrete (``0 == ANIM_TARGET``). Unlike the file-level form,
        an unresolvable condition here is a generator bug: silently choosing a
        branch would desynchronise the script.
        """
        expression = expression.strip()
        if not expression:
            raise MacroError("empty .if condition")
        try:
            return bool(self.symbols.evaluate(expression))
        except MacroError as error:
            raise MacroError(f"cannot evaluate .if {expression!r}: {error}") from error


# --------------------------------------------------------------------------
# Emission
# --------------------------------------------------------------------------

def render_bytes(tokens):
    return ", ".join(f"0x{value:02x}" for value in tokens)


def emit(assembler, out):
    out.write("/* Generated by tools/gen_battle_data.py. Do not edit. */\n\n")
    out.write("#include \"battle.h\"\n")
    out.write("#include \"battle_anim.h\"\n\n")

    for name, tokens in assembler.result.scripts:
        out.write(f"const u8 {name}[] = {{\n    {render_bytes(tokens)}\n}};\n\n")

    for name, entries in assembler.result.tables:
        # Pointer tables (`const u8 *const X[]`) and plain u16 value tables
        # (`gMovesWithQuietBGM`) both appear in the sources; the C declaration
        # decides which, since a same-width table of numbers is not a pointer
        # table and must keep its original integer element type.
        if name in assembler.u16_tables:
            out.write(f"const u16 {name}[] = {{\n")
            for entry in entries:
                out.write(f"    0x{assembler.symbols.evaluate(entry) & 0xFFFF:04x},\n")
            out.write("};\n\n")
            continue
        out.write(f"const u8 *const {name}[] = {{\n")
        for entry in entries:
            expression = assembler.symbols.pointer_expression(entry)
            out.write(f"    {expression},\n")
        out.write("};\n\n")


# --------------------------------------------------------------------------
# Entry point
# --------------------------------------------------------------------------

def main():
    constants = load_defines(__import__("pathlib").Path(ROOT))
    constants["REVISION"] = 0  # select the non-.A branch of `.if REVISION >= 0xA`
    # `.set NULL, 0` / `.set FALSE, 0` / `.set TRUE, 1` are declared in the
    # script sources themselves and are not C constants.
    constants.setdefault("NULL", 0)
    constants.setdefault("FALSE", 0)
    constants.setdefault("TRUE", 1)
    external_kinds = collect_external_kinds()

    pointer_tables = collect_pointer_tables()
    function_macros = collect_function_macros()
    resolve_function_macro_constants(constants, function_macros)
    # Generated files live in their own directory: src/data/ also holds
    # pret data headers (battle_anim.h, battle_moves.h) whose names would
    # shadow include/ for same-directory includes.
    out_dir = os.path.join(ROOT, "src/data/battle")
    os.makedirs(out_dir, exist_ok=True)

    # One shared pointer registry across all script sets: the interpreter
    # resolves an operand index without needing to know which set it came from.
    registry = PointerRegistry()
    u16_tables = collect_u16_tables()

    for set_name, (
        sources,
        c_path,
        table_name,
        macro_path,
        pointer_vars,
        track_cursor,
    ) in SCRIPT_SETS.items():
        macros = load_macros(macro_path)
        readers = derive_opcode_readers(
            c_path,
            table_name,
            KNOWN_SLOT_OVERRIDES.get(set_name, {}),
            pointer_vars,
            track_cursor,
        )
        slot_kinds = derive_macro_slot_kinds(macros, readers)

        labels = set()
        for rel in sources:
            labels |= set(
                re.findall(
                    r"^([A-Za-z_]\w*)\s*:",
                    open(os.path.join(ROOT, rel)).read(),
                    re.M,
                )
            )
        symbols = Symbols(
            constants,
            labels,
            external_kinds,
            collect_pointer_defines(),
            function_macros,
        )
        assembler = Assembler(
            macros,
            slot_kinds,
            symbols,
            constants,
            pointer_tables,
            registry,
            u16_tables,
        )
        assembler.assemble_files(sources)

        path = os.path.join(out_dir, f"{set_name}_data.h")
        with open(path, "w") as handle:
            emit(assembler, handle)
        scripts = len(assembler.result.scripts)
        tables = len(assembler.result.tables)
        print(
            f"{set_name}: {scripts} scripts, {tables} tables -> "
            f"{os.path.relpath(path, ROOT)}"
        )

    # The shared table is defined in its own translation unit: the declaration
    # lives in global.h (so every reader macro can resolve an index), while the
    # definition references the generated script labels below.
    includes = (
        '#include "global.h"\n'
        '#include "battle.h"\n'
        '#include "battle_anim.h"\n'
        '#include "battle_message.h"\n'
        '#include "data/battle/battle_data.h"\n'
        '#include "data/battle/anim_data.h"\n'
        '#include "data/battle/ai_data.h"\n'
    )
    # Resolve which symbols the table still needs declared, using the real
    # header text so an existing prototype is never duplicated.
    include_text = "".join(
        open(os.path.join(ROOT, rel), errors="ignore").read()
        for rel in ("include/battle.h", "include/battle_anim.h", "include/battle_message.h")
    )
    extra_decls = needed_declarations(registry.entries, include_text)
    table_path = os.path.join(out_dir, "ptr_table.c")
    with open(table_path, "w") as handle:
        handle.write("/* Generated by tools/gen_battle_data.py. Do not edit. */\n\n")
        handle.write(includes)
        handle.write("\n")
        handle.write("\n".join(extra_decls))
        handle.write("\n\n")
        handle.write("const void *const gNativeBattlePtrs[] = {\n")
        for expression in registry.entries:
            handle.write(f"    {expression},\n")
        handle.write("};\n\n")
        handle.write(f"const u32 gNativeBattlePtrsCount = {len(registry)};\n")
    print(f"pointer table: {len(registry)} entries -> src/data/battle/ptr_table.c")


if __name__ == "__main__":
    main()
