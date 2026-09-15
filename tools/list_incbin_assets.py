#!/usr/bin/env python3
"""List the INCBIN assets the native build must generate.

pret's build leans on implicit rules and hand-written lists; the native Makefile
needs the list explicitly so object compilation can wait for the assets it
INCBINs (a clean tree otherwise races and dies with "Failed to open").

The list is derived, not hand-maintained, so it cannot drift: every INCBIN in
src/ that git does not track is resolved against the build's own rules, and the
ones that can actually be produced are printed space-separated for
`$(shell tools/list_incbin_assets.py)`.

Resolution covers both shapes pret uses:
  * a single-source conversion (`x.4bpp` from `x.png`, `x.gbapal` from `x.pal`,
    and the compressed and chained forms on top of those), and
  * a target assembled from other generated assets (`graphics/unused/obi1.4bpp`
    is a `cat` of converted PNGs), which is why the rule files are parsed for
    explicit targets rather than looking only for a same-stem source file.

Assets whose inputs are absent and which no rule can build are reported on
stderr and left off the list, so a genuinely broken asset rule is visible
instead of silently skipped. This script must not invoke make: it runs from
inside the Makefile, and a nested `make -p` re-enters it.
"""

import os
import re
import subprocess
import sys

SCAN_ROOTS = ("src",)
# Rule files that define asset targets (transitively what Makefile.native
# includes for assets).
RULE_FILES = ("Makefile.native", "graphics_file_rules.mk", "tileset_rules.mk",
              "spritesheet_rules.mk", "tools/battle_assets.mk",
              "tools/anim_sprite_assets.mk", "tools/battle_extra_assets.mk")

# An explicit target line: "path:" at the start of a line (not a variable,
# not a pattern rule, not a recipe line).
TARGET_RE = re.compile(r'^([^\s:#=\t][^:#=\n]*?)\s*:(?!=)')
# Conversions the rule set provides, longest suffix first.
CONVERSION_SUFFIXES = (".4bpp.lz", ".8bpp.lz", ".gbapal.lz", ".bin.lz",
                       ".hwlatfont", ".fwlatfont", ".hwjpnfont", ".fwjpnfont",
                       ".4bpp", ".8bpp", ".1bpp", ".gbapal", ".lz")
SOURCE_SUFFIXES = (".png", ".pal", ".bin", ".bmp", ".jasc")


def tracked_files():
    """Paths git provides; these already exist and need no rule."""
    try:
        out = subprocess.run(["git", "ls-files"], capture_output=True, text=True)
        return set(out.stdout.split())
    except OSError:
        return set()


def rule_variables():
    """Simple `NAME := value` assignments from the rule files.

    The explicit-target scan must resolve things like `$(UNUSEDGFXDIR)/obi1.4bpp`
    to a concrete path, so the directory variables those rules use are read
    first. Assignments are plain (`:=`) and never reference other variables, so
    a single pass suffices.
    """
    variables = {}
    assign_re = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*)\s*:=\s*(.*?)\s*$')
    for path in RULE_FILES:
        if not os.path.exists(path):
            continue
        for line in open(path, encoding="utf-8", errors="ignore"):
            if line.startswith(("\t", "#")):
                continue
            match = assign_re.match(line)
            if match and "$" not in match.group(2):
                variables[match.group(1)] = match.group(2)
    return variables


def expand_vars(text, variables):
    """Substitute $(NAME) for known simple variables."""
    return re.sub(r'\$\(([A-Za-z_][A-Za-z0-9_]*)\)',
                  lambda m: variables.get(m.group(1), m.group(0)), text)


def explicit_targets():
    """Every concrete target named in the asset rule files."""
    variables = rule_variables()
    targets = set()
    for path in RULE_FILES:
        if not os.path.exists(path):
            continue
        for line in open(path, encoding="utf-8", errors="ignore"):
            if line.startswith(("\t", "#")):
                continue
            match = TARGET_RE.match(line)
            if not match:
                continue
            name = expand_vars(match.group(1).strip(), variables)
            if "$" in name:
                continue
            # Static pattern rules name several targets in one field; the
            # pattern itself ("%") is not a concrete target.
            for part in name.split():
                if part and "%" not in part:
                    targets.add(part)
    return targets


def incbin_targets():
    """Every path named by an INCBIN in the compiled sources and headers.

    INCBIN takes a variable number of paths -- `INCBIN_U8("a.4bpp", "b.4bpp")`
    concatenates two files into one symbol -- so all quoted arguments are
    collected, not just the first.
    """
    targets = set()
    call_re = re.compile(r'INCBIN_[A-Z0-9_]*\s*\(([^)]*)\)')
    for root in SCAN_ROOTS:
        for dirpath, _, filenames in os.walk(root):
            for name in filenames:
                if not name.endswith((".c", ".h")):
                    continue
                try:
                    text = open(os.path.join(dirpath, name),
                                encoding="utf-8", errors="ignore").read()
                except OSError:
                    continue
                for args in call_re.findall(text):
                    targets.update(re.findall(r'"([^"]+)"', args))
    return targets


def make_resolver(sources, explicit):
    """Return resolve(target) -> whether the build can produce it."""
    cache = {}

    def resolve(target, depth=0):
        if target in cache:
            return cache[target]
        if depth > 8:  # a conversion chain is at most a couple of steps
            return False
        cache[target] = False  # break cycles before recursing
        ok = False
        if target in explicit:
            ok = True
        else:
            for suffix in CONVERSION_SUFFIXES:
                if not target.endswith(suffix):
                    continue
                stem = target[: -len(suffix)]
                if stem in sources or any(stem + s in sources
                                          for s in SOURCE_SUFFIXES):
                    ok = True
                elif resolve(stem, depth + 1):
                    ok = True
                if ok:
                    break
        cache[target] = ok
        return ok

    return resolve


def main():
    if "--list-targets" in sys.argv:
        # The concrete targets named by the asset rule files. Makefile.native
        # uses these to attach the tool build to every asset recipe; pret's
        # static pattern rules target real paths, so nothing else does it. The
        # tools themselves are excluded: depending on gbagfx from the rule that
        # builds gbagfx is a cycle, which make drops with a warning and then
        # schedules the asset recipes before the tool exists.
        tool_paths = {"tools/gbagfx/gbagfx", "tools/preproc/preproc",
                      "tools/mapjson/mapjson", "tools/jsonproc/jsonproc"}
        targets = sorted(t for t in explicit_targets() if t not in tool_paths)
        sys.stdout.write(" ".join(targets))
        return

    sources = tracked_files()
    explicit = explicit_targets()
    resolve = make_resolver(sources, explicit)

    needed = sorted(t for t in incbin_targets() if t not in sources)
    ok, broken = [], []
    for target in needed:
        (ok if resolve(target) else broken).append(target)

    for target in broken:
        print(f"[assets] no build rule for {target}; skipping", file=sys.stderr)
    try:
        sys.stdout.write(" ".join(ok))
        sys.stdout.flush()
    except BrokenPipeError:
        # Closed by a downstream `head`/`grep -q`; not an error.
        os.dup2(os.open(os.devnull, os.O_WRONLY), sys.stdout.fileno())


if __name__ == "__main__":
    main()
