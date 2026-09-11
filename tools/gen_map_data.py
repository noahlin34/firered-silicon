#!/usr/bin/env python3
import json
import re
import sys
from pathlib import Path

# Native scenes are linked explicitly, just like ENGINE_SRCS. Compile their
# complete script dependency closure; other scenes retain their existing stubs.
NATIVE_SCRIPT_ROOTS = {
    "PalletTown_PlayersHouse_1F_EventScript_Mom",
    "PalletTown_PlayersHouse_1F_EventScript_TV",
    "PalletTown_PlayersHouse_2F_EventScript_NES",
    "PalletTown_PlayersHouse_2F_EventScript_Sign",
    "EventScript_Bookshelf",
    "EventScript_Cabinet",
    "EventScript_Dresser",
    "EventScript_Kitchen",
    "EventScript_PlayerFacingTVScreen",
    # Walk-away cancel: FieldInput_HandleCancelSignpost sets this up when the
    # player pushes a direction away from an open signpost message box. Without
    # the real script (special DoPicboxCancel, release, end) the box's frame
    # stays drawn on screen.
    "EventScript_CancelMessageBox",
    # Pallet Town exterior: A-press interactions (the two wandering NPCs and
    # every sign) and Oak's northern exit interception triggers.
    "PalletTown_EventScript_FatMan",
    "PalletTown_EventScript_OaksLabSign",
    "PalletTown_EventScript_PlayersHouseSign",
    "PalletTown_EventScript_RivalsHouseSign",
    "PalletTown_EventScript_SignLady",
    "PalletTown_EventScript_TownSign",
    "PalletTown_EventScript_TrainerTips",
    "PalletTown_EventScript_OakTriggerLeft",
    "PalletTown_EventScript_OakTriggerRight",
    # Oak's Lab interior: every A-press target the player can reach. The three
    # aides, the rival, the two dex units, the computer terminals, the two signs
    # and the three starter balls; Oak himself is wired with the starter-scene
    # closure further down.
    "PalletTown_ProfessorOaksLab_EventScript_Aide1",
    "PalletTown_ProfessorOaksLab_EventScript_Aide2",
    "PalletTown_ProfessorOaksLab_EventScript_Aide3",
    "PalletTown_ProfessorOaksLab_EventScript_Computer",
    "PalletTown_ProfessorOaksLab_EventScript_LeftSign",
    "PalletTown_ProfessorOaksLab_EventScript_RightSign",
    "PalletTown_ProfessorOaksLab_EventScript_Pokedex",
    "PalletTown_ProfessorOaksLab_EventScript_Rival",
    "PalletTown_ProfessorOaksLab_EventScript_BulbasaurBall",
    "PalletTown_ProfessorOaksLab_EventScript_SquirtleBall",
    "PalletTown_ProfessorOaksLab_EventScript_CharmanderBall",
    # Lab starter-scene closure. Oak's A-press script plus the
    # ON_TRANSITION/ON_WARP/ON_FRAME header tables (parsed separately below)
    # run the whole choose-a-starter flow: the interception cutscene leaves
    # the scene var at 1 and ON_FRAME then fires ChooseStarterScene, which
    # walks Oak to the dex desk and hands control to the player.
    # The _MapScripts header table itself is compiled by MapScriptRegistry
    # (below), not by ScriptRegistry; it is listed here so main() parses it.
    "PalletTown_ProfessorOaksLab_MapScripts",
    "PalletTown_ProfessorOaksLab_EventScript_ReadyPlayerForStarterScene",
    "PalletTown_ProfessorOaksLab_ChooseStarterScene",
    "PalletTown_ProfessorOaksLab_EventScript_LeaveStarterSceneTrigger",
    "PalletTown_ProfessorOaksLab_EventScript_ProfOak",
    "PalletTown_ProfessorOaksLab_EventScript_ConfirmStarterChoice",
    "PalletTown_ProfessorOaksLab_EventScript_ChoseStarter",
    "PalletTown_ProfessorOaksLab_EventScript_RivalPicksStarter",
    "PalletTown_ProfessorOaksLab_EventScript_ReadyEndSignLadyScene",
    "EventScript_GiveNicknameToStarter",
    "PalletTown_ProfessorOaksLab_EventScript_LastPokeBall",
    # Oak's Parcel chain: leaving the lab through Pallet Town and Route 1 into
    # Viridian City, the Mart's ON_FRAME parcel scene (which hands the player
    # ITEM_OAKS_PARCEL and sets the lab scene var to 5), and the route's
    # free-Potion clerk.
    "ViridianCity_Mart_MapScripts",
    "ViridianCity_Mart_OnLoad",
    "ViridianCity_Mart_OnFrame",
    "ViridianCity_Mart_EventScript_HideQuestionnaire",
    "ViridianCity_Mart_EventScript_ParcelScene",
    "ViridianCity_Mart_EventScript_Clerk",
    "ViridianCity_Mart_EventScript_SayHiToOak",
    "ViridianCity_Mart_Items",
    "ViridianCity_Mart_EventScript_Woman",
    "ViridianCity_Mart_EventScript_Youngster",
    "Route1_MapScripts",
    "Route1_EventScript_MartClerk",
    "Route1_EventScript_AlreadyGotPotion",
    "Route1_EventScript_Boy",
    "Route1_EventScript_RouteSign",
    # Viridian City is the road to the Mart: its ON_TRANSITION map script sets
    # the world-map flag and places the tutorial old man, and its signs and
    # NPCs are the A-press targets on the way.
    "ViridianCity_MapScripts",
    "ViridianCity_OnTransition",
    "ViridianCity_EventScript_SetOldManNormal",
    "ViridianCity_EventScript_SetOldManStandingByRoad",
    "ViridianCity_EventScript_SetOldManBlockingRoad",
    "ViridianCity_EventScript_TryUnlockGym",
    "ViridianCity_EventScript_GymDoorLocked",
    "ViridianCity_EventScript_RoadBlocked",
    "ViridianCity_EventScript_CitySign",
    "ViridianCity_EventScript_TrainerTips1",
    "ViridianCity_EventScript_TrainerTips2",
    "ViridianCity_EventScript_GymSign",
    "ViridianCity_EventScript_GymDoor",
    "ViridianCity_EventScript_Boy",
    "ViridianCity_EventScript_OldMan",
    "ViridianCity_EventScript_OldManGymLeaderReturned",
    "ViridianCity_EventScript_Youngster",
    "ViridianCity_EventScript_Woman",
    "ViridianCity_EventScript_WomanRoadBlocked",
}


CONNECTION_DIRECTIONS = {
    "up": "CONNECTION_NORTH",
    "down": "CONNECTION_SOUTH",
    "left": "CONNECTION_WEST",
    "right": "CONNECTION_EAST",
    "dive": "CONNECTION_DIVE",
    "emerge": "CONNECTION_EMERGE",
}


def is_global_script(name):
    return name.startswith("EventScript_") or name in NATIVE_SCRIPT_ROOTS


class UnsupportedScript(Exception):
    pass


def strip_comment(line):
    # Assembly comments use '@'. Text directives are parsed separately and retain
    # their contents verbatim.
    return line.split("@", 1)[0].strip()


def parse_args(rest):
    args = []
    current = []
    depth = 0
    for char in rest:
        if char == "," and depth == 0:
            args.append("".join(current).strip())
            current = []
            continue
        current.append(char)
        if char in "([":
            depth += 1
        elif char in ")]":
            depth -= 1
    if current or rest.strip():
        args.append("".join(current).strip())
    return args


def parse_labels(path, text=False):
    labels = {}
    macros = {}
    current = None
    current_macro = None
    macro_lines = []
    values = []
    for raw in path.read_text().splitlines():
        line = raw.strip()
        if not text and line.startswith(".macro"):
            parts = line.split(None, 1)
            current_macro = parts[1].strip() if len(parts) > 1 else ""
            macro_lines = []
            continue
        if not text and line.startswith(".endm"):
            if current_macro:
                macros[current_macro] = macro_lines
                current_macro = None
                macro_lines = []
            continue
        if not text and current_macro is not None:
            cleaned = strip_comment(line).strip()
            if cleaned:
                macro_lines.append(cleaned)
            continue
        match = re.match(r"^([A-Za-z_][A-Za-z0-9_]*):{1,2}(?:\s*@.*)?$", line)
        if match:
            if current is not None:
                labels[current] = values
            current = match.group(1)
            values = []
            continue
        if current is None:
            continue
        if text:
            match = re.match(r'^\.string\s+("(?:\\.|[^"\\])*")\s*$', line)
            if match:
                values.append(match.group(1)[1:-1])
        else:
            line = strip_comment(line)
            # `.include` is an assembler directive, not script data. The
            # included files are parsed on their own, so a label that happens to
            # precede one must not carry it as a command.
            if line.startswith(".include"):
                continue
            if line in macros:
                values.extend(macros[line])
            elif line:
                values.append(line)
    if current is not None:
        labels[current] = values
    return labels


def load_defines(root):
    raw_defines = {}
    for path in (root / "include").glob("**/*.h"):
        for raw in path.read_text(errors="ignore").splitlines():
            match = re.match(r"\s*#define\s+([A-Za-z_][A-Za-z0-9_]*)\s+(.+?)\s*$", raw)
            if match and "(" not in match.group(1):
                raw_defines[match.group(1)] = match.group(2).split("//", 1)[0].strip()

    values = {"TRUE": 1, "FALSE": 0, "MALE": 0, "FEMALE": 1}
    values.update({
        "MSGBOX_NPC": 2,
        "MSGBOX_SIGN": 3,
        "MSGBOX_DEFAULT": 4,
        "MSGBOX_YESNO": 5,
        "MSGBOX_AUTOCLOSE": 6,
        "EQUAL": 1,
        "TRUE": 1,
        "FALSE": 0,
        # asm/macros/event.inc: YES = 1, NO = 0 (compared against VAR_RESULT
        # after a MSGBOX_YESNO).
        "YES": 1,
        "NO": 0,
    })

    # Resolve the simple integer constants used by event scripts. Complex C
    # expressions remain unavailable and are reported as unsupported commands.
    for _ in range(len(raw_defines) + 1):
        changed = False
        for name, expression in raw_defines.items():
            if name in values:
                continue
            expr = re.sub(r"\b(?:u8|u16|u32|s8|s16|s32)\b", "", expression)
            # Strip C integer suffixes from literals only. Removing every 'U'
            # mangles identifiers (MAX_TRAINERS_COUNT -> MAX_TRAINERS_CONT),
            # which silently drops every constant derived from SYS_FLAGS.
            expr = re.sub(r"\b(0[xX][0-9a-fA-F]+|\d+)[uUlL]+\b", r"\1", expr)
            names = set(re.findall(r"\b[A-Za-z_][A-Za-z0-9_]*\b", expr))
            if any(name not in values for name in names):
                continue
            try:
                values[name] = int(eval(expr, {"__builtins__": {}}, values))
                changed = True
            except (SyntaxError, TypeError, ValueError, NameError, OverflowError):
                continue
        if not changed:
            break
    return values


def load_external_scripts(root):
    path = root / "src/platform/overworld_stubs.c"
    if not path.exists():
        return set()
    names = set()
    for match in re.finditer(r"\b(?:const\s+)?u8\s+([A-Za-z_][A-Za-z0-9_]*)\s*\[", path.read_text()):
        names.add(match.group(1))
    return names


def load_movement_actions(root):
    # asm/macros/movement.inc maps each movement mnemonic to the single
    # MOVEMENT_ACTION_* byte the script stores.
    actions = {}
    path = root / "asm/macros/movement.inc"
    for raw in path.read_text().splitlines():
        match = re.match(r"\s*create_movement_action\s+([a-z_][a-z0-9_]*)\s*,\s*([A-Za-z_][A-Za-z0-9_]*)", raw)
        if match:
            actions[match.group(1)] = match.group(2)
    return actions


def collect_equates(root):
    # Map scripts define local symbols with .equ (e.g. SIGN_LADY_READY).
    equates = {}
    paths = sorted((root / "data/maps").glob("*/scripts.inc"))
    paths += sorted((root / "data/scripts").glob("**/*.inc"))
    for path in paths:
        for raw in path.read_text().splitlines():
            match = re.match(r"\s*\.(?:equ|set)\s+([A-Za-z_][A-Za-z0-9_]*)\s*,\s*(.+?)\s*$", raw)
            if match:
                equates.setdefault(match.group(1), strip_comment(match.group(2)))
    return equates


def resolve_equates(root, constants):
    equates = collect_equates(root)
    for _ in range(len(equates) + 1):
        changed = False
        for name, expression in equates.items():
            if name in constants:
                continue
            referenced = set(re.findall(r"\b[A-Za-z_][A-Za-z0-9_]*\b", expression))
            if any(symbol not in constants for symbol in referenced):
                continue
            try:
                constants[name] = int(eval(expression, {"__builtins__": {}}, constants))
            except (SyntaxError, TypeError, ValueError, NameError, OverflowError):
                continue
            changed = True
        if not changed:
            break


def resolve_map_script_constant(constants, name):
    value = constants.get(name)
    if value is None:
        try:
            return int(name, 0)
        except ValueError:
            raise UnsupportedScript(f"map script constant {name} is not available")
    return value


def little_endian(value, size):
    if value < 0 or value >= (1 << (size * 8)):
        raise UnsupportedScript(f"value {value} does not fit in {size} bytes")
    return [(value >> (8 * i)) & 0xff for i in range(size)]


def split_command(line):
    parts = line.split(None, 1)
    if not parts:
        return "", []
    return parts[0], parse_args(parts[1] if len(parts) == 2 else "")


class TextRegistry:
    def __init__(self, sources):
        self.sources = sources
        self.generated = []
        self.seen = set()
        # Labels emitted with external linkage for linked engine files.
        self.generated_globals = set()

    def reference(self, label):
        if label not in self.sources:
            raise UnsupportedScript(f"text label {label} is not available")
        if label not in self.seen:
            self.seen.add(label)
            self.generated.append(label)
        return label

    def reference_global(self, label):
        """Emit a text label that a linked engine file references by extern.

        Engine sources compiled from C (not through tools/preproc, so their
        `_("...")` strings are not available) expect these symbols to exist in
        the generated data, exactly as they did in the GBA build's text banks.
        """
        if label not in self.sources:
            raise UnsupportedScript(f"text label {label} is not available")
        self.generated_globals.add(label)
        # Appended to `generated` so the definition is emitted; the modifiers in
        # emit()/the declaration loop switch to external linkage for this label.
        if label not in self.seen:
            self.seen.add(label)
            self.generated.append(label)
        return label

    def emit(self, out):
        for label in self.generated:
            fragments = self.sources[label]
            modifier = "const" if label in self.generated_globals else "static const"
            out.write(f"{modifier} u8 {label}[] = _(\n")
            for fragment in fragments:
                out.write(f'    "{fragment}"\n')
            out.write(");\n\n")

    def emit_declarations(self, out):
        for label in sorted(self.generated_globals):
            out.write(f"extern const u8 {label}[];\n")


def collect_engine_text_references(root, text_sources):
    """Text labels that linked engine .c files reference by `extern const u8`.

    Engine sources built through plain CFLAGS do not go through tools/preproc,
    so their `_("...")` strings are not compiled; the generated text bank must
    supply these symbols with external linkage (for example src/prof_pc.c's
    PokedexRating_Text_* rating messages). Labels that are not text (scripts,
    movements) are ignored here and emitted by their own registries.
    """
    labels = set()
    for path in sorted((root / "src").rglob("*.c")):
        for raw in path.read_text(errors="ignore").splitlines():
            match = re.match(r"\s*extern\s+const\s+u8\s+([A-Za-z_][A-Za-z0-9_]*)\s*\[\s*\]\s*;", raw)
            if match and match.group(1) in text_sources:
                labels.add(match.group(1))
    return labels


class MovementRegistry:
    def __init__(self, sources, constants, actions):
        self.sources = sources
        self.constants = constants
        self.actions = actions
        self.generated = []
        self.seen = set()
        self.failed = {}

    def reference(self, label):
        if label not in self.sources:
            raise UnsupportedScript(f"movement label {label} is not available")
        if label not in self.seen:
            self.seen.add(label)
            self.generated.append(label)
        return label

    def compile(self, label):
        output = []
        for line in self.sources[label]:
            command, args = split_command(line)
            if args:
                raise UnsupportedScript(f"movement command {command} takes no arguments")
            constant = self.actions.get(command)
            if constant is None:
                raise UnsupportedScript(f"movement command {command} is not supported")
            if constant not in self.constants:
                raise UnsupportedScript(f"movement constant {constant} is not available")
            output.append(self.constants[constant])
        return output

    def emit(self, out):
        for label in self.generated:
            values = self.compile(label)
            out.write(f"static const u8 {label}[] = {{\n")
            out.write("    " + ", ".join(f"0x{value:02x}" for value in values) + ",\n")
            out.write("};\n\n")


class ScriptRegistry:
    def __init__(self, sources, constants, text_registry, movement_registry, external):
        self.sources = sources
        self.constants = constants
        self.text_registry = text_registry
        self.movement_registry = movement_registry
        self.external = external
        self.generated = []
        self.seen = set()
        self.compiling = set()
        self.failed = {}
        self.external_references = set()

    def resolve(self, value):
        value = value.strip()
        if value in self.constants:
            return self.constants[value]
        if re.fullmatch(r"[-+]?0[xX][0-9a-fA-F]+|[-+]?\d+", value):
            return int(value, 0)
        raise UnsupportedScript(f"constant {value} is not available")

    def resolve_pointer(self, label):
        label = label.strip()
        if label in self.sources:
            return self.ensure(label)
        if label in self.external:
            self.external_references.add(label)
            return label
        raise UnsupportedScript(f"script label {label} is not available")

    def ensure(self, label):
        if label in self.external:
            self.external_references.add(label)
            return label
        if label not in self.sources:
            raise UnsupportedScript(f"script label {label} is not available")
        if label in self.seen:
            return label
        if label in self.compiling:
            return label
        self.compiling.add(label)
        self.compile(label)
        self.compiling.remove(label)
        self.seen.add(label)
        self.generated.append(label)
        return label

    # The linker requires pointer relocations to be aligned, but script
    # operands are stored unaligned. References therefore become u32 indices
    # into an aligned pointer table; the interpreter resolves them at runtime.
    def pointer_bytes(self, expression):
        if expression == "NULL":
            return little_endian(0, 4)
        return little_endian(self.pointer_index(expression), 4)

    def pointer_index(self, expression):
        if not hasattr(self, "_ptr_table"):
            self._ptr_table = []
        if expression not in self._ptr_table:
            self._ptr_table.append(expression)
        return self._ptr_table.index(expression)

    def pointer_for_message(self, expression):
        expression = expression.strip()
        if expression in ("0", "NULL"):
            return self.pointer_bytes("NULL")
        return self.pointer_bytes(self.text_registry.reference(expression))

    def pointer_for_script(self, expression):
        return self.pointer_bytes(self.resolve_pointer(expression))

    def pointer_for_movement(self, expression):
        return self.pointer_bytes(self.movement_registry.reference(expression))

    def branch(self, opcode, condition, destination):
        return [opcode, condition] + self.pointer_for_script(destination)

    def compare_branch(self, opcode, condition, left, right, destination):
        left_value = self.resolve(left) if isinstance(left, str) else left
        right_value = self.resolve(right) if isinstance(right, str) else right
        left_is_var = left_value >= 0x4000
        right_is_var = right_value >= 0x4000
        if left_is_var and right_is_var:
            compare = [0x22] + little_endian(left_value, 2) + little_endian(right_value, 2)
        elif left_is_var and not right_is_var:
            compare = [0x21] + little_endian(left_value, 2) + little_endian(right_value, 2)
        else:
            raise UnsupportedScript("comparison requires a variable on the left")
        return compare + self.branch(opcode, condition, destination)

    def msgbox(self, text, msgbox_type):
        values = []
        if msgbox_type == "MSGBOX_NPC":
            values += [0x6a, 0x5a]
        elif msgbox_type == "MSGBOX_SIGN":
            values += [0x69]
        elif msgbox_type == "MSGBOX_YESNO":
            # Std_MsgboxYesNo (data/scripts/std_msgbox.inc): message, waitmessage,
            # yesnobox 20, 8. Inlined so the box does not depend on gStdScripts,
            # which this generator does not emit.
            values += [0x67] + self.pointer_for_message(text) + [0x66, 0x6e, 20, 8]
            return values
        elif msgbox_type != "MSGBOX_DEFAULT":
            raise UnsupportedScript(f"message box type {msgbox_type} is not supported")
        values += [0x67] + self.pointer_for_message(text) + [0x66, 0x6d]
        if msgbox_type == "MSGBOX_NPC":
            values += [0x6c]
        elif msgbox_type == "MSGBOX_SIGN":
            values += [0x6b]
        return values

    def compile(self, label):
        output = []
        for line in self.sources[label]:
            command, args = split_command(line)
            try:
                if command == "nop" and not args:
                    output.append(0x00)
                elif command == "end" and not args:
                    output.append(0x02)
                elif command == "return" and not args:
                    output.append(0x03)
                elif command == "call" and len(args) == 1:
                    output += [0x04] + self.pointer_for_script(args[0])
                elif command == "goto" and len(args) == 1:
                    output += [0x05] + self.pointer_for_script(args[0])
                elif command == "goto_if" and len(args) == 2:
                    output += self.branch(0x06, self.resolve(args[0]), args[1])
                elif command == "call_if" and len(args) == 2:
                    output += self.branch(0x07, self.resolve(args[0]), args[1])
                elif command == "goto_if_set" and len(args) == 2:
                    output += [0x2b] + little_endian(self.resolve(args[0]), 2)
                    output += self.branch(0x06, 1, args[1])
                elif command == "goto_if_unset" and len(args) == 2:
                    output += [0x2b] + little_endian(self.resolve(args[0]), 2)
                    output += self.branch(0x06, 0, args[1])
                elif command == "call_if_set" and len(args) == 2:
                    output += [0x2b] + little_endian(self.resolve(args[0]), 2)
                    output += self.branch(0x07, 1, args[1])
                elif command == "call_if_unset" and len(args) == 2:
                    output += [0x2b] + little_endian(self.resolve(args[0]), 2)
                    output += self.branch(0x07, 0, args[1])
                elif command in {"goto_if_eq", "goto_if_ne", "goto_if_lt", "goto_if_gt", "goto_if_le", "goto_if_ge"} and len(args) == 3:
                    conditions = {
                        "goto_if_lt": 0,
                        "goto_if_eq": 1,
                        "goto_if_gt": 2,
                        "goto_if_le": 3,
                        "goto_if_ge": 4,
                        "goto_if_ne": 5,
                    }
                    output += self.compare_branch(0x06, conditions[command], args[0], args[1], args[2])
                elif command in {"call_if_eq", "call_if_ne", "call_if_lt", "call_if_gt", "call_if_le", "call_if_ge"} and len(args) == 3:
                    conditions = {
                        "call_if_lt": 0,
                        "call_if_eq": 1,
                        "call_if_gt": 2,
                        "call_if_le": 3,
                        "call_if_ge": 4,
                        "call_if_ne": 5,
                    }
                    output += self.compare_branch(0x07, conditions[command], args[0], args[1], args[2])
                elif command == "checkplayergender" and not args:
                    output.append(0xa0)
                elif command == "lock" and not args:
                    output.append(0x6a)
                elif command == "lockall" and not args:
                    output.append(0x69)
                elif command == "faceplayer" and not args:
                    output.append(0x5a)
                elif command == "closemessage" and not args:
                    output.append(0x68)
                elif command == "release" and not args:
                    output.append(0x6c)
                elif command == "releaseall" and not args:
                    output.append(0x6b)
                elif command == "waitmessage" and not args:
                    output.append(0x66)
                elif command == "waitbuttonpress" and not args:
                    output.append(0x6d)
                elif command == "waitfanfare" and not args:
                    output.append(0x32)
                elif command == "waitmovement" and len(args) == 1:
                    output += [0x51] + little_endian(self.resolve(args[0]), 2)
                elif command == "applymovement" and len(args) >= 2:
                    output += [0x4f] + little_endian(self.resolve(args[0]), 2)
                    output += self.pointer_for_movement(args[1])
                    if len(args) > 2:
                        raise UnsupportedScript("applymovement with an explicit map is not supported")
                elif command == "playfanfare" and len(args) == 1:
                    output += [0x31] + little_endian(self.resolve(args[0]), 2)
                elif command == "fadescreen" and len(args) == 1:
                    output += [0x97, self.resolve(args[0])]
                elif command == "special" and len(args) == 1:
                    special_index = self.constants.get("SPECIAL_" + args[0])
                    if special_index is None:
                        raise UnsupportedScript(f"special {args[0]} is not available")
                    output += [0x25] + little_endian(special_index, 2)
                elif command == "setflag" and len(args) == 1:
                    output += [0x29] + little_endian(self.resolve(args[0]), 2)
                elif command == "clearflag" and len(args) == 1:
                    output += [0x2a] + little_endian(self.resolve(args[0]), 2)
                elif command == "checkflag" and len(args) == 1:
                    output += [0x2b] + little_endian(self.resolve(args[0]), 2)
                elif command == "setvar" and len(args) == 2:
                    output += [0x16] + little_endian(self.resolve(args[0]), 2) + little_endian(self.resolve(args[1]), 2)
                elif command == "compare_var_to_value" and len(args) == 2:
                    output += [0x21] + little_endian(self.resolve(args[0]), 2) + little_endian(self.resolve(args[1]), 2)
                elif command == "compare_var_to_var" and len(args) == 2:
                    output += [0x22] + little_endian(self.resolve(args[0]), 2) + little_endian(self.resolve(args[1]), 2)
                elif command == "message" and len(args) == 1:
                    output += [0x67] + self.pointer_for_message(args[0])
                elif command == "msgbox" and len(args) in (1, 2):
                    output += self.msgbox(args[0], args[1] if len(args) == 2 else "MSGBOX_DEFAULT")
                elif command == "loadword" and len(args) == 2:
                    output.append(0x0f)
                    output.append(self.resolve(args[0]))
                    if args[1] in self.sources or args[1] in self.external:
                        output += self.pointer_for_script(args[1])
                    elif args[1] in self.text_registry.sources:
                        output += self.pointer_for_message(args[1])
                    else:
                        output += little_endian(self.resolve(args[1]), 8)
                elif command == ".byte" and args:
                    for arg in args:
                        output.append(self.resolve(arg))
                elif command == ".2byte" and args:
                    for arg in args:
                        output += little_endian(self.resolve(arg), 2)
                elif command == ".4byte" and args:
                    for arg in args:
                        if arg in self.sources or arg in self.external:
                            output += self.pointer_for_script(arg)
                        else:
                            output += little_endian(self.resolve(arg), 4)
                elif command == "playse" and len(args) == 1:
                    output += [0x2f] + little_endian(self.resolve(args[0]), 2)
                elif command == "copyobjectxytoperm" and len(args) == 1:
                    output += [0x64] + little_endian(self.resolve(args[0]), 2)
                elif command == "famechecker" and len(args) in (2, 3):
                    # famechecker person, index [, function]
                    function = args[2] if len(args) == 3 else "SetFlavorTextFlagFromSpecialVars"
                    special_index = self.constants.get("SPECIAL_" + function)
                    if special_index is None:
                        raise UnsupportedScript(f"special {function} is not available")
                    output += [0x16] + little_endian(self.resolve("VAR_0x8004"), 2) + little_endian(self.resolve(args[0]), 2)
                    output += [0x16] + little_endian(self.resolve("VAR_0x8005"), 2) + little_endian(self.resolve(args[1]), 2)
                    output += [0x25] + little_endian(special_index, 2)
                elif command == "delay" and len(args) == 1:
                    output += [0x28] + little_endian(self.resolve(args[0]), 2)
                elif command == "turnobject" and len(args) == 2:
                    output += [0x5b] + little_endian(self.resolve(args[0]), 2) + [self.resolve(args[1])]
                elif command == "savebgm" and len(args) == 1:
                    output += [0x34] + little_endian(self.resolve(args[0]), 2)
                elif command == "fadedefaultbgm" and not args:
                    output.append(0x35)
                elif command == "textcolor" and len(args) == 1:
                    output += [0xc7, self.resolve(args[0])]
                elif command == "copyvar" and len(args) == 2:
                    output += [0x19] + little_endian(self.resolve(args[0]), 2) + little_endian(self.resolve(args[1]), 2)
                elif command == "waitstate" and not args:
                    output.append(0x27)
                elif command == "removeobject" and len(args) == 1:
                    output += [0x53] + little_endian(self.resolve(args[0]), 2)
                elif command == "showmonpic" and len(args) == 3:
                    output += [0x75] + little_endian(self.resolve(args[0]), 2)
                    output += [self.resolve(args[1]), self.resolve(args[2])]
                elif command == "hidemonpic" and not args:
                    output.append(0x76)
                elif command == "givemon" and len(args) in (2, 3):
                    item = self.resolve(args[2]) if len(args) == 3 else self.resolve("ITEM_NONE")
                    output += [0x79] + little_endian(self.resolve(args[0]), 2) + [self.resolve(args[1])]
                    output += little_endian(item, 2) + [0x00] * 9
                elif command == "bufferspeciesname" and len(args) == 2:
                    string_vars = {"STR_VAR_1": 0, "STR_VAR_2": 1, "STR_VAR_3": 2}
                    if args[0] not in string_vars:
                        raise UnsupportedScript(f"string var {args[0]} is not supported")
                    output += [0x7d, string_vars[args[0]]] + little_endian(self.resolve(args[1]), 2)
                elif command == "signmsg" and not args:
                    output.append(0xca)
                elif command == "normalmsg" and not args:
                    output.append(0xcb)
                elif command == "playbgm" and len(args) == 2:
                    output += [0x33] + little_endian(self.resolve(args[0]), 2) + [self.resolve(args[1])]
                elif command == "addobject" and len(args) == 1:
                    output += [0x55] + little_endian(self.resolve(args[0]), 2)
                elif command == "opendoor" and len(args) == 2:
                    output += [0xac] + little_endian(self.resolve(args[0]), 2) + little_endian(self.resolve(args[1]), 2)
                elif command == "closedoor" and len(args) == 2:
                    output += [0xad] + little_endian(self.resolve(args[0]), 2) + little_endian(self.resolve(args[1]), 2)
                elif command == "waitdooranim" and not args:
                    output.append(0xae)
                elif command == "setworldmapflag" and len(args) == 1:
                    output += [0xd0] + little_endian(self.resolve(args[0]), 2)
                elif command == "setobjectxyperm" and len(args) == 3:
                    output += [0x63] + little_endian(self.resolve(args[0]), 2) + little_endian(self.resolve(args[1]), 2) + little_endian(self.resolve(args[2]), 2)
                elif command == "setobjectmovementtype" and len(args) == 2:
                    output += [0x65] + little_endian(self.resolve(args[0]), 2) + [self.resolve(args[1])]
                elif command == "specialvar" and len(args) == 2:
                    special_index = self.constants.get("SPECIAL_" + args[1])
                    if special_index is None:
                        raise UnsupportedScript(f"special {args[1]} is not available")
                    output += [0x26] + little_endian(self.resolve(args[0]), 2) + little_endian(special_index, 2)
                elif command == "buffernumberstring" and len(args) == 2:
                    string_vars = {"STR_VAR_1": 0, "STR_VAR_2": 1, "STR_VAR_3": 2}
                    if args[0] not in string_vars:
                        raise UnsupportedScript(f"string var {args[0]} is not supported")
                    output += [0x83, string_vars[args[0]]] + little_endian(self.resolve(args[1]), 2)
                elif command == "warp" and len(args) in (2, 3, 4):
                    map_id = self.resolve(args[0])
                    map_group = (map_id >> 8) & 0xff
                    map_num = map_id & 0xff
                    if len(args) == 3:
                        warp_id = 0xff
                        x = self.resolve(args[1])
                        y = self.resolve(args[2])
                    elif len(args) == 4:
                        warp_id = self.resolve(args[1])
                        x = self.resolve(args[2])
                        y = self.resolve(args[3])
                    else:
                        raise UnsupportedScript("warp with explicit warp id is not supported")
                    output += [0x39, map_group, map_num, warp_id] + little_endian(x, 2) + little_endian(y, 2)
                elif command == "checkitem" and len(args) in (1, 2):
                    quantity = self.resolve(args[1]) if len(args) == 2 else 1
                    output += [0x47] + little_endian(self.resolve(args[0]), 2) + little_endian(quantity, 2)
                elif command == "checkitemspace" and len(args) in (1, 2):
                    quantity = self.resolve(args[1]) if len(args) == 2 else 1
                    output += [0x46] + little_endian(self.resolve(args[0]), 2) + little_endian(quantity, 2)
                elif command == "bufferitemname" and len(args) == 2:
                    string_vars = {"STR_VAR_1": 0, "STR_VAR_2": 1, "STR_VAR_3": 2}
                    if args[0] not in string_vars:
                        raise UnsupportedScript(f"string var {args[0]} is not supported")
                    output += [0x80, string_vars[args[0]]] + little_endian(self.resolve(args[1]), 2)
                elif command == "setmetatile" and len(args) == 4:
                    output += [0xa2] + little_endian(self.resolve(args[0]), 2) + little_endian(self.resolve(args[1]), 2)
                    output += little_endian(self.resolve(args[2]), 2) + little_endian(self.resolve(args[3]), 2)
                elif command == "pokemart" and len(args) == 1:
                    # The item list is a `label:: .2byte ITEM_*, ITEM_NONE` block;
                    # compiled as a plain script array it already is the u16 list
                    # ScrCmd_pokemart reads (it stops at ITEM_NONE, so the trailing
                    # release/end bytes after the terminator are never reached).
                    output += [0x86] + self.pointer_for_script(args[0])
                elif command == ".align" and len(args) == 1:
                    # Alignment padding for the GBA's u16 tables; the native
                    # arrays are byte-packed and the interpreter reads through
                    # ctx->scriptPtr, so no padding is emitted.
                    pass
                elif command == "additem" and len(args) in (1, 2):
                    quantity = self.resolve(args[1]) if len(args) == 2 else 1
                    output += [0x44] + little_endian(self.resolve(args[0]), 2) + little_endian(quantity, 2)
                elif command == "removeitem" and len(args) in (1, 2):
                    quantity = self.resolve(args[1]) if len(args) == 2 else 1
                    output += [0x45] + little_endian(self.resolve(args[0]), 2) + little_endian(quantity, 2)
                elif command == "goto_if_questlog" and len(args) == 1:
                    # asm/macros/event.inc: special GetQuestLogState,
                    # compare VAR_RESULT, 2, goto_if_eq dest.
                    special_index = self.constants.get("SPECIAL_GetQuestLogState")
                    if special_index is None:
                        raise UnsupportedScript("special GetQuestLogState is not available")
                    output += [0x26] + little_endian(self.resolve("VAR_RESULT"), 2) + little_endian(special_index, 2)
                    output += self.compare_branch(0x06, 1, "VAR_RESULT", 2, args[0])
                elif command == "compare" and len(args) == 2:
                    # asm/macros/event.inc: compare_var_to_var when the right
                    # side is a var/special, else compare_var_to_value.
                    right = self.resolve(args[1])
                    if right >= 0x4000:
                        output += [0x22] + little_endian(self.resolve(args[0]), 2) + little_endian(right, 2)
                    else:
                        output += [0x21] + little_endian(self.resolve(args[0]), 2) + little_endian(right, 2)
                elif command == "map_script_2" and len(args) == 3:
                    # Conditional-entry sub-table (ON_WARP_INTO_MAP_TABLE /
                    # ON_FRAME_TABLE) referenced by a map_script header entry.
                    var = resolve_map_script_constant(self.constants, args[0])
                    compare = resolve_map_script_constant(self.constants, args[1])
                    self.ensure(args[2])
                    output += little_endian(var, 2) + little_endian(compare, 2)
                    output += self.pointer_bytes(args[2])
                elif command == ".2byte" and args == ["0"]:
                    output.append(0x00)  # sub-table terminator
                elif command == "giveitem_msg" and len(args) in (2, 3, 4):
                    # asm/macros/event.inc: additem + loadword + three
                    # setorcopyvar + callstd STD_RECEIVED_ITEM. The std script
                    # (Std_ReceivedItem) is not compiled natively, so the
                    # closure is inlined: fanfare, message, wait, waitfanfare,
                    # restore text color.
                    amount = self.resolve(args[2]) if len(args) >= 3 else 1
                    fanfare = self.resolve(args[3]) if len(args) == 4 else self.resolve("MUS_LEVEL_UP")
                    output += [0x44] + little_endian(self.resolve(args[1]), 2) + little_endian(amount, 2)
                    output += [0x0f, 0x00] + self.pointer_for_message(args[0])
                    output += [0x1a] + little_endian(self.resolve("VAR_0x8000"), 2) + little_endian(self.resolve(args[1]), 2)
                    output += [0x1a] + little_endian(self.resolve("VAR_0x8001"), 2) + little_endian(amount, 2)
                    output += [0x1a] + little_endian(self.resolve("VAR_0x8002"), 2) + little_endian(fanfare, 2)
                    if fanfare == self.resolve("MUS_LEVEL_UP"):
                        output += [0x31] + little_endian(self.resolve("MUS_LEVEL_UP"), 2)
                    elif fanfare == self.resolve("MUS_OBTAIN_KEY_ITEM"):
                        output += [0x31] + little_endian(self.resolve("MUS_OBTAIN_KEY_ITEM"), 2)
                    else:
                        raise UnsupportedScript(f"giveitem_msg fanfare {fanfare} is not supported")
                    output += [0x67] + self.pointer_for_message(args[0]) + [0x66, 0x32]
                    # call EventScript_RestorePrevTextColor
                    output += [0x04] + self.pointer_for_script("EventScript_RestorePrevTextColor")
                else:
                    raise UnsupportedScript(f"command {command} is not supported")
            except (IndexError, KeyError, ValueError) as error:
                raise UnsupportedScript(f"malformed {command}: {error}") from error
        return output

    def emit(self, out):
        # Scripts are plain u8 arrays; pointer references are u32 indices into
        # the aligned gNativeScriptPtrs table emitted by emit_declarations.
        for label in self.generated:
            values = self.compile(label)
            modifier = "const" if is_global_script(label) else "static const"
            out.write(f"{modifier} u8 {label}[] = {{\n")
            rendered = [f"0x{value:02x}" if isinstance(value, int) else value for value in values]
            out.write("    " + ", ".join(rendered) + "\n")
            out.write("};\n\n")

    def emit_declarations(self, out):
        # Aligned so each entry holds a genuine linker-resolved 64-bit address.
        out.write("const void *const gNativeScriptPtrs[] = {\n")
        for expression in self._ptr_table:
            out.write(f"    {expression},\n")
        out.write("};\n\n")


class MapScriptRegistry:
    """Compiles map_script/map_script_2 header tables into native C.

    GBA assemblers emit the script pointer inside a packed `.4byte`; on the
    host those bytes must resolve through the aligned gNativeScriptPtrs table
    (see fix #23), so the table is emitted as a u8 array too. The pointer
    indices come from the script registry's shared table, and the referenced
    labels are ensured there as plain scripts.
    """

    def __init__(self, script_registry):
        self.scripts = script_registry
        self.generated = []

    def compile(self, map_name, map_scripts):
        """map_scripts: list of (tag, label) plain entries and
        ('table', var, compare, label) conditional entries, in source order."""
        output = []
        for entry in map_scripts:
            if entry[0] == "table":
                _, var, compare, label = entry
                output += little_endian(var, 2)
                output += little_endian(compare, 2)
                output += self.scripts.pointer_bytes(label)
            else:
                tag, label = entry
                output.append(tag)
                output += self.scripts.pointer_bytes(label)
        output.append(0x00)
        self.generated.append((f"{map_name}_MapScripts", output))

    def emit(self, out):
        for name, values in self.generated:
            out.write(f"static const u8 {name}[] = {{\n")
            out.write("    " + ", ".join(f"0x{value:02x}" for value in values) + "\n")
            out.write("};\n\n")

def collect_sources(root):
    script_sources = {}
    for path in sorted((root / "data/maps").glob("*/scripts.inc")):
        script_sources.update(parse_labels(path))
    for path in sorted((root / "data/scripts").glob("**/*.inc")):
        if path.name != "movement.inc":
            script_sources.update(parse_labels(path))
    script_sources.update(parse_labels(root / "data/event_scripts.s"))

    # Movement sequences live both in data/scripts/movement.inc and beside the
    # map scripts that use them, so both sets are movement sources.
    movement_sources = dict(script_sources)
    movement_sources.update(parse_labels(root / "data/scripts/movement.inc"))

    text_sources = {}
    for path in sorted((root / "data").glob("**/text.inc")):
        text_sources.update(parse_labels(path, text=True))
    for path in sorted((root / "data/text").glob("**/*.inc")):
        text_sources.update(parse_labels(path, text=True))
    # Event scripts define shared text inline (e.g. Text_GiveNicknameToThisMon)
    # alongside their scripts; without these the message operands cannot resolve.
    # Only labels that actually carry .string data are merged, and an existing
    # data/text/*.inc definition wins.
    for label, fragments in parse_labels(root / "data/event_scripts.s", text=True).items():
        if fragments:
            text_sources.setdefault(label, fragments)
    return script_sources, movement_sources, text_sources


def collect_local_ids(root):
    local_ids = {"LOCALID_NONE": 0, "LOCALID_PLAYER": 255}
    for path in sorted((root / "data/maps").glob("*/map.json")):
        try:
            map_data = json.loads(path.read_text())
        except json.JSONDecodeError:
            continue
        for index, obj in enumerate(map_data.get("object_events", []), 1):
            name = obj.get("local_id")
            if name and name.isidentifier():
                local_ids.setdefault(name, index)
    return local_ids


def collect_specials(root, constants):
    path = root / "data/specials.inc"
    index = 0
    in_table = False
    for raw in path.read_text().splitlines():
        if raw.strip() == "gSpecials::":
            in_table = True
            continue
        if not in_table:
            continue
        match = re.match(r"\s*def_special\s+([A-Za-z_][A-Za-z0-9_]*)", raw)
        if match:
            constants.setdefault("SPECIAL_" + match.group(1), index)
            index += 1


def main():
    root = Path(__file__).resolve().parent.parent
    layouts_json = json.loads((root / "data/layouts/layouts.json").read_text())
    groups_json = json.loads((root / "data/maps/map_groups.json").read_text())

    out_layouts_file = root / "src/data/layouts_data.h"
    out_maps_file = root / "src/data/maps_data.h"

    # -------------------------------------------------------------
    # Generate layouts_data.h
    # -------------------------------------------------------------
    with open(out_layouts_file, "w") as f:
        f.write("/* Auto-generated by tools/gen_map_data.py - DO NOT EDIT */\n\n")
        f.write('#include "global.h"\n')
        f.write('#include "global.fieldmap.h"\n')
        f.write('#include "constants/layouts.h"\n\n')

        tilesets = set()
        for layout in layouts_json["layouts"]:
            if not layout:
                continue
            if layout.get("primary_tileset") and layout["primary_tileset"] != "NULL":
                tilesets.add(layout["primary_tileset"])
            if layout.get("secondary_tileset") and layout["secondary_tileset"] != "NULL":
                tilesets.add(layout["secondary_tileset"])
        for ts in sorted(tilesets):
            f.write(f"extern const struct Tileset {ts};\n")
        f.write("\n")

        for layout in layouts_json["layouts"]:
            if not layout:
                continue
            name = layout["name"]
            b_path = layout["border_filepath"]
            m_path = layout["blockdata_filepath"]
            f.write(f"static const u16 {name}_Border[] = INCBIN_U16(\"{b_path}\");\n")
            f.write(f"static const u16 {name}_Blockdata[] = INCBIN_U16(\"{m_path}\");\n\n")

            pri = f"&{layout['primary_tileset']}" if layout.get('primary_tileset') and layout['primary_tileset'] != "NULL" else "NULL"
            sec = f"&{layout['secondary_tileset']}" if layout.get('secondary_tileset') and layout['secondary_tileset'] != "NULL" else "NULL"

            f.write(f"const struct MapLayout {name} = {{\n")
            f.write(f"    .width = {layout['width']},\n")
            f.write(f"    .height = {layout['height']},\n")
            f.write(f"    .border = {name}_Border,\n")
            f.write(f"    .map = {name}_Blockdata,\n")
            f.write(f"    .primaryTileset = {pri},\n")
            f.write(f"    .secondaryTileset = {sec},\n")
            f.write(f"    .borderWidth = {layout.get('border_width', 2)},\n")
            f.write(f"    .borderHeight = {layout.get('border_height', 2)},\n")
            f.write("};\n\n")

        f.write("const struct MapLayout *const gMapLayouts[] = {\n")
        for layout in layouts_json["layouts"]:
            if layout and layout.get("name"):
                f.write(f"    &{layout['name']},\n")
            else:
                f.write("    NULL,\n")
        f.write("};\n\n")

    print(f"Wrote {out_layouts_file}")

    layout_id_to_name = {}
    for layout in layouts_json["layouts"]:
        if layout and layout.get("id"):
            layout_id_to_name[layout["id"]] = layout["name"]
    all_map_names = []
    for grp in groups_json["group_order"]:
        for map_name in groups_json[grp]:
            all_map_names.append(map_name)

    constants = load_defines(root)
    constants.update(collect_local_ids(root))
    resolve_equates(root, constants)
    collect_specials(root, constants)
    script_sources, movement_sources, text_sources = collect_sources(root)
    movement_actions = load_movement_actions(root)
    external_scripts = load_external_scripts(root)

    # Compile every requested scene on its own: one unsupported command must not
    # take down the other scenes, and a scene that fails to compile keeps its
    # dummy-script fallback instead of being wired into the map data.
    compiled_roots = set()
    for script_name in sorted(NATIVE_SCRIPT_ROOTS):
        if script_name.endswith("_MapScripts"):
            continue  # compiled by MapScriptRegistry below
        probe = ScriptRegistry(
            script_sources,
            constants,
            TextRegistry(text_sources),
            MovementRegistry(movement_sources, constants, movement_actions),
            external_scripts,
        )
        try:
            probe.ensure(script_name)
        except UnsupportedScript as error:
            print(f"gen_map_data: {script_name} left stubbed: {error}", file=sys.stderr)
            continue
        compiled_roots.add(script_name)

    # Map script header tables (map_script / map_script_2) for maps whose
    # events were compiled natively. Parsed from each map's scripts.inc after
    map_script_tables = {}
    for script_name in sorted(NATIVE_SCRIPT_ROOTS):
        if not script_name.endswith("_MapScripts"):
            continue
        map_name = script_name[:-len("_MapScripts")]
        map_scripts_path = root / f"data/maps/{map_name}/scripts.inc"
        if not map_scripts_path.exists():
            print(f"gen_map_data: {script_name} left stubbed: no scripts.inc", file=sys.stderr)
            continue
        entries = []
        ok = True
        in_table = False
        for raw in map_scripts_path.read_text().splitlines():
            line = strip_comment(raw).strip()
            if not in_table:
                if line.startswith(script_name + "::"):
                    in_table = True
                continue
            if re.match(r"^[A-Za-z_][A-Za-z0-9_]*::", line) or not line:
                break  # next label: table ended
            if line.startswith("."):
                if line == ".byte 0":
                    break  # explicit terminator
                continue  # .equ and friends
            command, args = split_command(line)
            try:
                if command == "map_script" and len(args) == 2:
                    tag = resolve_map_script_constant(constants, args[0])
                    entries.append((tag, args[1]))
                elif command == "map_script_2" and len(args) == 3:
                    var = resolve_map_script_constant(constants, args[0])
                    compare = resolve_map_script_constant(constants, args[1])
                    entries.append(("table", var, compare, args[2]))
                else:
                    raise UnsupportedScript(f"unsupported map script line: {line}")
            except UnsupportedScript as error:
                print(f"gen_map_data: {script_name} left stubbed: {error}", file=sys.stderr)
                ok = False
                break
        if ok:
            map_script_tables[map_name] = entries

    text_registry = TextRegistry(text_sources)
    for label in sorted(collect_engine_text_references(root, text_sources)):
        text_registry.reference_global(label)
    movement_registry = MovementRegistry(movement_sources, constants, movement_actions)
    script_registry = ScriptRegistry(
        script_sources,
        constants,
        text_registry,
        movement_registry,
        external_scripts,
    )

    for script_name in sorted(compiled_roots):
        script_registry.ensure(script_name)
    map_script_registry = MapScriptRegistry(script_registry)
    for map_name, entries in map_script_tables.items():
        try:
            for entry in entries:
                label = entry[3] if entry[0] == "table" else entry[1]
                script_registry.ensure(label)
            map_script_registry.compile(map_name, entries)
        except UnsupportedScript as error:
            print(f"gen_map_data: {map_name}_MapScripts left stubbed: {error}", file=sys.stderr)

    map_data = {}
    for map_name in all_map_names:
        map_path = root / f"data/maps/{map_name}/map.json"
        if not map_path.exists():
            continue
        m = json.loads(map_path.read_text())
        map_data[map_name] = m

    with open(out_maps_file, "w") as f:
        f.write("/* Auto-generated by tools/gen_map_data.py - DO NOT EDIT */\n\n")
        f.write('#include "global.h"\n')
        f.write('#include "global.fieldmap.h"\n')
        f.write('#include "event_scripts.h"\n')
        f.write('#include "constants/maps.h"\n')
        f.write('#include "constants/map_groups.h"\n')
        f.write('#include "constants/region_map_sections.h"\n')
        f.write('#include "constants/songs.h"\n')
        f.write('#include "constants/weather.h"\n')
        f.write('#include "constants/event_objects.h"\n')
        f.write('#include "constants/event_object_movement.h"\n')
        f.write('#include "constants/trainer_types.h"\n\n')
        f.write('#include "constants/event_bg.h"\n')
        f.write("static const u8 sDummyScript[] = { 0x02 };\n")
        f.write("static const u8 sEmptyMapScripts[] = { 0x00 };\n")
        for name in movement_registry.generated:
            f.write(f"static const u8 {name}[];\n")
        if script_registry.external_references:
            f.write("\n")
        for name in script_registry.generated:
            modifier = "extern const" if is_global_script(name) else "static const"
            f.write(f"{modifier} u8 {name}[];\n")
        for name in text_registry.generated:
            modifier = "extern const" if name in text_registry.generated_globals else "static const"
            f.write(f"{modifier} u8 {name}[];\n")
        for name in movement_registry.generated:
            f.write(f"static const u8 {name}[];\n")
        script_registry.emit_declarations(f)
        if script_registry.generated or text_registry.generated or movement_registry.generated:
            f.write("\n")
        text_registry.emit(f)
        movement_registry.emit(f)
        script_registry.emit(f)
        map_script_registry.emit(f)

        def event_script(event, fallback="sDummyScript"):
            script_name = event.get("script")
            if script_name not in compiled_roots:
                return fallback
            return f"(const u8 *)&{script_name}"

        for map_name in all_map_names:
            m = map_data.get(map_name)
            if m is None:
                continue

            obj_events = m.get("object_events", [])
            if obj_events:
                f.write(f"static const struct ObjectEventTemplate {map_name}_ObjectEvents[] = {{\n")
                for i, obj in enumerate(obj_events):
                    f.write("    {\n")
                    f.write(f"        .localId = {i + 1},\n")
                    f.write(f"        .graphicsId = {obj.get('graphics_id', '0')},\n")
                    f.write("        .kind = 0,\n")
                    f.write(f"        .x = {obj.get('x', 0)},\n")
                    f.write(f"        .y = {obj.get('y', 0)},\n")
                    f.write("        .objUnion = {\n")
                    f.write("            .normal = {\n")
                    f.write(f"                .elevation = {obj.get('elevation', 0)},\n")
                    f.write(f"                .movementType = {obj.get('movement_type', '0')},\n")
                    f.write(f"                .movementRangeX = {obj.get('movement_range_x', 0)},\n")
                    f.write(f"                .movementRangeY = {obj.get('movement_range_y', 0)},\n")
                    f.write(f"                .trainerType = {obj.get('trainer_type', '0')},\n")
                    f.write(f"                .trainerRange_berryTreeId = {obj.get('trainer_sight_or_berry_tree_id', '0')},\n")
                    f.write("            }\n")
                    f.write("        },\n")
                    f.write(f"        .script = {event_script(obj)},\n")
                    f.write(f"        .flagId = {obj.get('flag', '0')},\n")
                    f.write("    },\n")
                f.write("};\n\n")

            warp_events = m.get("warp_events", [])
            if warp_events:
                f.write(f"static const struct WarpEvent {map_name}_WarpEvents[] = {{\n")
                for warp in warp_events:
                    f.write("    {\n")
                    f.write(f"        .x = {warp.get('x', 0)},\n")
                    f.write(f"        .y = {warp.get('y', 0)},\n")
                    f.write(f"        .elevation = {warp.get('elevation', 0)},\n")
                    f.write(f"        .warpId = {warp.get('dest_warp_id', 0)},\n")
                    dest_map = warp.get('dest_map', 'MAP_NONE')
                    if dest_map != 'MAP_NONE':
                        f.write(f"        .mapNum = MAP_NUM({dest_map}),\n")
                        f.write(f"        .mapGroup = MAP_GROUP({dest_map}),\n")
                    else:
                        f.write("        .mapNum = 0,\n")
                        f.write("        .mapGroup = 0,\n")
                    f.write("    },\n")
                f.write("};\n\n")

            coord_events = m.get("coord_events", [])
            if coord_events:
                f.write(f"static const struct CoordEvent {map_name}_CoordEvents[] = {{\n")
                for coord in coord_events:
                    f.write("    {\n")
                    f.write(f"        .x = {coord.get('x', 0)},\n")
                    f.write(f"        .y = {coord.get('y', 0)},\n")
                    f.write(f"        .elevation = {coord.get('elevation', 0)},\n")
                    f.write(f"        .trigger = {coord.get('var', '0')},\n")
                    f.write(f"        .index = {coord.get('var_value', '0')},\n")
                    # A coord event is an automatic proximity trigger: a dummy
                    # script would re-fire every frame and livelock the player.
                    f.write(f"        .script = {event_script(coord, 'NULL')},\n")
                    f.write("    },\n")
                f.write("};\n\n")

            bg_events = m.get("bg_events", [])
            if bg_events:
                f.write(f"static const struct BgEvent {map_name}_BgEvents[] = {{\n")
                for bg in bg_events:
                    f.write("    {\n")
                    f.write(f"        .x = {bg.get('x', 0)},\n")
                    f.write(f"        .y = {bg.get('y', 0)},\n")
                    f.write(f"        .elevation = {bg.get('elevation', 0)},\n")
                    f.write(f"        .kind = {bg.get('player_facing_dir', '0')},\n")
                    f.write(f"        .bgUnion = {{ .script = {event_script(bg)} }},\n")
                    f.write("    },\n")
                f.write("};\n\n")

            # Map connections: outdoors maps need these to render their
            # neighbours and to allow walking across the shared border.
            connections = m.get("connections") or []
            if connections:
                f.write(f"static const struct MapConnection {map_name}_Connections[] = {{\n")
                for connection in connections:
                    direction = CONNECTION_DIRECTIONS[connection.get("direction", "")]
                    dest_map = connection["map"]
                    f.write("    {\n")
                    f.write(f"        .direction = {direction},\n")
                    f.write(f"        .offset = {connection.get('offset', 0)},\n")
                    f.write(f"        .mapGroup = MAP_GROUP({dest_map}),\n")
                    f.write(f"        .mapNum = MAP_NUM({dest_map}),\n")
                    f.write("    },\n")
                f.write("};\n\n")
                f.write(f"static const struct MapConnections {map_name}_MapConnections = {{\n")
                f.write(f"    .count = {len(connections)},\n")
                f.write(f"    .connections = {map_name}_Connections,\n")
                f.write("};\n\n")
            connections_name = f"&{map_name}_MapConnections" if connections else "NULL"

            has_shared_events = "shared_events_map" in m
            if not has_shared_events:
                f.write(f"const struct MapEvents {map_name}_MapEvents = {{\n")
                f.write(f"    .objectEventCount = {len(obj_events)},\n")
                f.write(f"    .warpCount = {len(warp_events)},\n")
                f.write(f"    .coordEventCount = {len(coord_events)},\n")
                f.write(f"    .bgEventCount = {len(bg_events)},\n")
                f.write(f"    .objectEvents = {'&' + map_name + '_ObjectEvents[0]' if obj_events else 'NULL'},\n")
                f.write(f"    .warps = {'&' + map_name + '_WarpEvents[0]' if warp_events else 'NULL'},\n")
                f.write(f"    .coordEvents = {'&' + map_name + '_CoordEvents[0]' if coord_events else 'NULL'},\n")
                f.write(f"    .bgEvents = {'&' + map_name + '_BgEvents[0]' if bg_events else 'NULL'},\n")
                f.write("};\n\n")
            layout_id = m.get('layout', 'LAYOUT_PALLET_TOWN_PLAYERS_HOUSE_2F')
            layout_name = layout_id_to_name.get(layout_id, "PalletTown_PlayersHouse_2F_Layout")
            f.write(f"extern const struct MapLayout {layout_name};\n")
            ev_name = m["shared_events_map"] if has_shared_events else map_name
            f.write(f"extern const struct MapEvents {ev_name}_MapEvents;\n")

            f.write(f"const struct MapHeader {map_name} = {{\n")
            f.write(f"    .mapLayout = &{layout_name},\n")
            f.write(f"    .events = &{ev_name}_MapEvents,\n")
            map_scripts_name = f"{map_name}_MapScripts" if map_name in map_script_tables else "sEmptyMapScripts"
            f.write(f"    .mapScripts = {map_scripts_name},\n")
            f.write(f"    .connections = {connections_name},\n")
            f.write(f"    .music = {m.get('music', 'MUS_PALLET')},\n")
            f.write(f"    .mapLayoutId = {layout_id},\n")
            f.write(f"    .regionMapSectionId = {m.get('region_map_section', 'MAPSEC_PALLET_TOWN')},\n")
            f.write(f"    .cave = {1 if m.get('requires_flash') else 0},\n")
            f.write(f"    .weather = {m.get('weather', 'WEATHER_NONE')},\n")
            f.write(f"    .mapType = {m.get('map_type', 'MAP_TYPE_INDOOR')},\n")
            f.write(f"    .bikingAllowed = {1 if m.get('allow_cycling') else 0},\n")
            f.write(f"    .allowEscaping = {1 if m.get('allow_escaping') else 0},\n")
            f.write(f"    .allowRunning = {1 if m.get('allow_running') else 0},\n")
            f.write(f"    .showMapName = {1 if m.get('show_map_name') else 0},\n")
            f.write(f"    .floorNum = {m.get('floor_number', 0)},\n")
            f.write(f"    .battleType = {m.get('battle_scene', 'MAP_BATTLE_SCENE_NORMAL')},\n")
            f.write("};\n\n")

        for grp in groups_json["group_order"]:
            maps_in_grp = groups_json[grp]
            f.write(f"static const struct MapHeader *const gMapGroup_{grp}[] = {{\n")
            for map_name in maps_in_grp:
                f.write(f"    &{map_name},\n")
            f.write("};\n\n")

        f.write("const struct MapHeader *const *const gMapGroups[] = {\n")
        for grp in groups_json["group_order"]:
            f.write(f"    gMapGroup_{grp},\n")
        f.write("};\n\n")

    print(f"Wrote {out_maps_file}")


if __name__ == "__main__":
    main()
