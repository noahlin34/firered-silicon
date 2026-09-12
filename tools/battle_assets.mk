# Battle engine graphics generated from the checked-in PNG, PAL, and BIN sources.
# Keep this include independent so it can also be invoked directly from the repo root.
GFX ?= tools/gbagfx/gbagfx

# Required INCBIN targets in src/battle*.c.  The raw BIN entries below are
# source assets already present in the repository; all other targets are built
# by the rules below (with uncompressed intermediates where needed).
BATTLE_4BPP_OUTPUTS := \
	graphics/battle_anims/unused/water.4bpp \
	graphics/battle_interface/unused.4bpp \
	graphics/battle_records/bg_tiles.4bpp \
	graphics/battle_transitions/big_pokeball.4bpp \
	graphics/battle_transitions/grid_square.4bpp \
	graphics/battle_transitions/mugshot_banner.4bpp \
	graphics/battle_transitions/sliding_pokeball.4bpp \
	graphics/battle_transitions/unused_brendan.4bpp \
	graphics/battle_transitions/unused_lass.4bpp

BATTLE_4BPP_LZ := \
	graphics/battle_interface/level_up_banner.4bpp.lz \
	graphics/battle_terrain/building/anim.4bpp.lz \
	graphics/battle_terrain/building/terrain.4bpp.lz \
	graphics/battle_terrain/cave/anim.4bpp.lz \
	graphics/battle_terrain/cave/terrain.4bpp.lz \
	graphics/battle_terrain/grass/anim.4bpp.lz \
	graphics/battle_terrain/grass/terrain.4bpp.lz \
	graphics/battle_terrain/indoor/terrain.4bpp.lz \
	graphics/battle_terrain/longgrass/anim.4bpp.lz \
	graphics/battle_terrain/longgrass/terrain.4bpp.lz \
	graphics/battle_terrain/mountain/anim.4bpp.lz \
	graphics/battle_terrain/mountain/terrain.4bpp.lz \
	graphics/battle_terrain/pond/anim.4bpp.lz \
	graphics/battle_terrain/pond/terrain.4bpp.lz \
	graphics/battle_terrain/sand/anim.4bpp.lz \
	graphics/battle_terrain/sand/terrain.4bpp.lz \
	graphics/battle_terrain/underwater/anim.4bpp.lz \
	graphics/battle_terrain/underwater/terrain.4bpp.lz \
	graphics/battle_terrain/water/anim.4bpp.lz \
	graphics/battle_terrain/water/terrain.4bpp.lz

# The .4bpp files in this list are intermediates for compressed outputs, plus
# the uncompressed target files listed in BATTLE_4BPP_OUTPUTS.
BATTLE_4BPP_FROM_PNG := \
	$(BATTLE_4BPP_OUTPUTS) \
	graphics/learn_move/interface_sprites.4bpp \
	graphics/battle_interface/level_up_banner.4bpp \
	graphics/battle_terrain/building/anim.4bpp \
	graphics/battle_terrain/building/terrain.4bpp \
	graphics/battle_terrain/cave/anim.4bpp \
	graphics/battle_terrain/cave/terrain.4bpp \
	graphics/battle_terrain/grass/anim.4bpp \
	graphics/battle_terrain/grass/terrain.4bpp \
	graphics/battle_terrain/indoor/terrain.4bpp \
	graphics/battle_terrain/longgrass/anim.4bpp \
	graphics/battle_terrain/longgrass/terrain.4bpp \
	graphics/battle_terrain/mountain/anim.4bpp \
	graphics/battle_terrain/mountain/terrain.4bpp \
	graphics/battle_terrain/pond/anim.4bpp \
	graphics/battle_terrain/pond/terrain.4bpp \
	graphics/battle_terrain/sand/anim.4bpp \
	graphics/battle_terrain/sand/terrain.4bpp \
	graphics/battle_terrain/underwater/anim.4bpp \
	graphics/battle_terrain/underwater/terrain.4bpp \
	graphics/battle_terrain/water/anim.4bpp \
	graphics/battle_terrain/water/terrain.4bpp

BATTLE_GBAPAL_OUTPUTS := \
	graphics/battle_anims/unused/flying.gbapal \
	graphics/battle_anims/unused/unknown.gbapal \
	graphics/battle_interface/level_up_banner.gbapal \
	graphics/battle_records/bg_tiles.gbapal \
	graphics/battle_transitions/agatha_bg.gbapal \
	graphics/battle_transitions/blue_bg.gbapal \
	graphics/battle_transitions/bruno_bg.gbapal \
	graphics/battle_transitions/green_bg.gbapal \
	graphics/battle_transitions/lance_bg.gbapal \
	graphics/battle_transitions/lorelei_bg.gbapal \
	graphics/battle_transitions/red_bg.gbapal \
	graphics/battle_transitions/sliding_pokeball.gbapal \
	graphics/battle_transitions/unused_trainer.gbapal

BATTLE_GBAPAL_LZ := \
	graphics/battle_terrain/building/terrain.gbapal.lz \
	graphics/battle_terrain/cave/terrain.gbapal.lz \
	graphics/battle_terrain/grass/terrain.gbapal.lz \
	graphics/battle_terrain/indoor/1.gbapal.lz \
	graphics/battle_terrain/indoor/2.gbapal.lz \
	graphics/battle_terrain/indoor/agatha.gbapal.lz \
	graphics/battle_terrain/indoor/bruno.gbapal.lz \
	graphics/battle_terrain/indoor/champion.gbapal.lz \
	graphics/battle_terrain/indoor/gym.gbapal.lz \
	graphics/battle_terrain/indoor/lance.gbapal.lz \
	graphics/battle_terrain/indoor/leader.gbapal.lz \
	graphics/battle_terrain/indoor/link.gbapal.lz \
	graphics/battle_terrain/indoor/lorelei.gbapal.lz \
	graphics/battle_terrain/indoor/plain.gbapal.lz \
	graphics/battle_terrain/longgrass/terrain.gbapal.lz \
	graphics/battle_terrain/mountain/terrain.gbapal.lz \
	graphics/battle_terrain/pond/terrain.gbapal.lz \
	graphics/battle_terrain/sand/terrain.gbapal.lz \
	graphics/battle_terrain/underwater/terrain.gbapal.lz \
	graphics/battle_terrain/water/terrain.gbapal.lz

# Palette intermediates include the uncompressed files needed by BATTLE_GBAPAL_LZ.
BATTLE_GBAPAL_FROM_PAL := \
	graphics/battle_anims/unused/flying.gbapal \
	graphics/battle_anims/unused/unknown.gbapal \
	graphics/battle_terrain/building/terrain.gbapal \
	graphics/battle_terrain/cave/terrain.gbapal \
	graphics/battle_terrain/grass/terrain.gbapal \
	graphics/battle_terrain/indoor/1.gbapal \
	graphics/battle_terrain/indoor/2.gbapal \
	graphics/battle_terrain/indoor/agatha.gbapal \
	graphics/battle_terrain/indoor/bruno.gbapal \
	graphics/battle_terrain/indoor/champion.gbapal \
	graphics/battle_terrain/indoor/gym.gbapal \
	graphics/battle_terrain/indoor/lance.gbapal \
	graphics/battle_terrain/indoor/leader.gbapal \
	graphics/battle_terrain/indoor/link.gbapal \
	graphics/battle_terrain/indoor/lorelei.gbapal \
	graphics/battle_terrain/indoor/plain.gbapal \
	graphics/battle_terrain/longgrass/terrain.gbapal \
	graphics/battle_terrain/mountain/terrain.gbapal \
	graphics/battle_terrain/pond/terrain.gbapal \
	graphics/battle_terrain/sand/terrain.gbapal \
	graphics/battle_terrain/underwater/terrain.gbapal \
	graphics/battle_terrain/water/terrain.gbapal \
	graphics/battle_transitions/agatha_bg.gbapal \
	graphics/battle_transitions/blue_bg.gbapal \
	graphics/battle_transitions/bruno_bg.gbapal \
	graphics/battle_transitions/green_bg.gbapal \
	graphics/battle_transitions/lance_bg.gbapal \
	graphics/battle_transitions/lorelei_bg.gbapal \
	graphics/battle_transitions/red_bg.gbapal \
	graphics/battle_transitions/sliding_pokeball.gbapal \
	graphics/battle_transitions/unused_trainer.gbapal

BATTLE_GBAPAL_FROM_PNG := \
	graphics/learn_move/interface_sprites.gbapal \
	graphics/battle_interface/level_up_banner.gbapal \
	graphics/battle_records/bg_tiles.gbapal

BATTLE_BIN_LZ := \
	graphics/battle_terrain/building/anim.bin.lz \
	graphics/battle_terrain/building/terrain.bin.lz \
	graphics/battle_terrain/cave/anim.bin.lz \
	graphics/battle_terrain/cave/terrain.bin.lz \
	graphics/battle_terrain/grass/anim.bin.lz \
	graphics/battle_terrain/grass/terrain.bin.lz \
	graphics/battle_terrain/indoor/terrain.bin.lz \
	graphics/battle_terrain/longgrass/anim.bin.lz \
	graphics/battle_terrain/longgrass/terrain.bin.lz \
	graphics/battle_terrain/mountain/anim.bin.lz \
	graphics/battle_terrain/mountain/terrain.bin.lz \
	graphics/battle_terrain/pond/anim.bin.lz \
	graphics/battle_terrain/pond/terrain.bin.lz \
	graphics/battle_terrain/sand/anim.bin.lz \
	graphics/battle_terrain/sand/terrain.bin.lz \
	graphics/battle_terrain/underwater/anim.bin.lz \
	graphics/battle_terrain/underwater/terrain.bin.lz \
	graphics/battle_terrain/water/anim.bin.lz \
	graphics/battle_terrain/water/terrain.bin.lz

BATTLE_SOURCE_BIN := \
	graphics/battle_anims/unused/water.bin \
	graphics/battle_records/bg_tiles.bin \
	graphics/battle_transitions/big_pokeball_tilemap.bin \
	graphics/battle_transitions/sliding_pokeball.bin \
	graphics/battle_transitions/vsbar_tilemap.bin

BATTLE_REQUIRED := \
	$(BATTLE_4BPP_OUTPUTS) \
	$(BATTLE_4BPP_LZ) \
	$(BATTLE_GBAPAL_OUTPUTS) \
	$(BATTLE_GBAPAL_LZ) \
	$(BATTLE_BIN_LZ) \
	$(BATTLE_SOURCE_BIN) \
	graphics/learn_move/interface_sprites.4bpp \
	graphics/learn_move/interface_sprites.gbapal

.PHONY: battle-assets
battle-assets: $(BATTLE_REQUIRED)
	@for asset in $(BATTLE_REQUIRED); do test -e "$$asset" || exit 1; done

$(BATTLE_4BPP_FROM_PNG): %.4bpp: %.png
	@mkdir -p $(dir $@)
	@$(GFX) $< $@

$(BATTLE_GBAPAL_FROM_PAL): %.gbapal: %.pal
	@mkdir -p $(dir $@)
	@$(GFX) $< $@

$(BATTLE_GBAPAL_FROM_PNG): %.gbapal: %.png
	@mkdir -p $(dir $@)
	@$(GFX) $< $@

$(BATTLE_4BPP_LZ): %.4bpp.lz: %.4bpp
	@mkdir -p $(dir $@)
	@$(GFX) $< $@

$(BATTLE_GBAPAL_LZ): %.gbapal.lz: %.gbapal
	@mkdir -p $(dir $@)
	@$(GFX) $< $@

$(BATTLE_BIN_LZ): %.bin.lz: %.bin
	@mkdir -p $(dir $@)
	@$(GFX) $< $@
