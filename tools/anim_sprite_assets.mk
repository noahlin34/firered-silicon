# Battle animation sprite tile data generated from the checked-in PNG sources.
# The target list is derived from the INCBIN references so it cannot drift.
GFX ?= tools/gbagfx/gbagfx

ANIM_SPRITE_DIR := graphics/battle_anims/sprites
ANIM_SPRITE_TARGETS := $(shell grep -oE 'graphics/battle_anims/sprites/[^"]+\.4bpp\.lz' src/graphics.c | sort -u)
ANIM_SPRITE_SPLIT_NAMES := flower ice_crystals ice_cube mud_sand spark
ANIM_SPRITE_SPLIT_4BPP := $(ANIM_SPRITE_SPLIT_NAMES:%=$(ANIM_SPRITE_DIR)/%.4bpp)
ANIM_SPRITE_4BPP := $(ANIM_SPRITE_TARGETS:.4bpp.lz=.4bpp)

# Keep the uncompressed files so a second invocation only rebuilds the five
# deliberately unconditional split sheets.
.SECONDARY: $(ANIM_SPRITE_4BPP)

.PHONY: anim-sprite-assets $(ANIM_SPRITE_SPLIT_4BPP)
anim-sprite-assets: $(ANIM_SPRITE_TARGETS)
	@for asset in $(ANIM_SPRITE_TARGETS); do test -s "$$asset" || exit 1; done

# Battle animation sheets are already arranged in the engine's tile order.
# The tool is an order-only prerequisite: these are pattern rules, so make may
# start a conversion while gbagfx is still compiling, and the recipe would run
# before the binary exists on a clean tree.
$(ANIM_SPRITE_DIR)/%.4bpp: $(ANIM_SPRITE_DIR)/%.png | $(GFX)
	@mkdir -p $(dir $@)
	@$(GFX) $< $@

$(ANIM_SPRITE_DIR)/%.4bpp.lz: $(ANIM_SPRITE_DIR)/%.4bpp | $(GFX)
	@mkdir -p $(dir $@)
	@$(GFX) $< $@

# The five split sheets (flower, ice_crystals, ice_cube, mud_sand, spark) are
# built by graphics_file_rules.mk, which converts each part and concatenates the
# tile data. That is the layout the engine reads: gBattleAnimPicTable declares
# flower/mud_sand as 0x00a0 (5 tiles), spark 0x0300, ice_cube 0x1200 -- the sum
# of the parts. Do not re-define those targets here; an earlier combine-into-one
# -PNG rule produced a much larger sheet than the engine reads.
