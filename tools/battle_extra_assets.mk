# Extra graphics used by the native battle-linked menu and animation scenes.
# Keep this include independent so it can also be invoked directly from the repo root.
GFX ?= tools/gbagfx/gbagfx

BATTLE_EXTRA_DIRECT_4BPP := \
	graphics/interface/select_button.4bpp \
	graphics/pokedex/blit_wide_ellipse.4bpp \
	graphics/pokedex/caught_marker.4bpp \
	graphics/quest_log/description_window.4bpp

BATTLE_EXTRA_4BPP_LZ := \
	graphics/pokemon_special_anim/bg.4bpp.lz \
	graphics/pokemon_special_anim/level_up.4bpp.lz \
	graphics/pokemon_special_anim/outward_spiral_dots.4bpp.lz \
	graphics/pokemon_special_anim/star.4bpp.lz \
	graphics/pokedex/mini_page.4bpp.lz \
	graphics/pokedex/kanto_dex_bgtiles.4bpp.lz \
	graphics/pokedex/national_dex_bgtiles.4bpp.lz \
	graphics/pokedex/cat_icon_cancel.4bpp.lz \
	graphics/pokedex/cat_icon_cave.4bpp.lz \
	graphics/pokedex/cat_icon_forest.4bpp.lz \
	graphics/pokedex/cat_icon_grassland.4bpp.lz \
	graphics/pokedex/cat_icon_lightest.4bpp.lz \
	graphics/pokedex/cat_icon_mountain.4bpp.lz \
	graphics/pokedex/cat_icon_numerical.4bpp.lz \
	graphics/pokedex/cat_icon_qmark.4bpp.lz \
	graphics/pokedex/cat_icon_rare.4bpp.lz \
	graphics/pokedex/cat_icon_rough_terrain.4bpp.lz \
	graphics/pokedex/cat_icon_sea.4bpp.lz \
	graphics/pokedex/cat_icon_smallest.4bpp.lz \
	graphics/pokedex/cat_icon_type.4bpp.lz \
	graphics/pokedex/cat_icon_urban.4bpp.lz \
	graphics/pokedex/cat_icon_waters_edge.4bpp.lz \
	graphics/pokedex/map_five_island.4bpp.lz \
	graphics/pokedex/map_four_island.4bpp.lz \
	graphics/pokedex/map_kanto.4bpp.lz \
	graphics/pokedex/map_one_island.4bpp.lz \
	graphics/pokedex/map_seven_island.4bpp.lz \
	graphics/pokedex/map_six_island.4bpp.lz \
	graphics/pokedex/map_three_island.4bpp.lz \
	graphics/pokedex/map_two_island.4bpp.lz \
	graphics/summary_screen/move_selection_cursor_left.4bpp.lz \
	graphics/summary_screen/move_selection_cursor_right.4bpp.lz \
	graphics/summary_screen/pokerus_cured.4bpp.lz \
	graphics/summary_screen/shiny_star.4bpp.lz

BATTLE_EXTRA_GBAPAL_FROM_PAL := \
	graphics/pokemon_special_anim/bg.gbapal \
	graphics/pokemon_special_anim/bg_tm_hm.gbapal \
	graphics/pokedex/kanto_dex_bgpals.gbapal \
	graphics/pokedex/national_dex_bgpals.gbapal \
	graphics/pokedex/silhouette_sprite_pal.gbapal \
	graphics/summary_screen/hp_bar_red.gbapal \
	graphics/summary_screen/hp_bar_yellow.gbapal \
	graphics/summary_screen/marking.gbapal \
	graphics/summary_screen/move_selection_cursor.gbapal \
	graphics/summary_screen/text_header.gbapal \
	graphics/summary_screen/text_moves.gbapal

BATTLE_EXTRA_GBAPAL_FROM_PNG := \
	graphics/pokemon_special_anim/level_up.gbapal \
	graphics/pokemon_special_anim/outward_spiral_dots.gbapal \
	graphics/pokemon_special_anim/star.gbapal \
	graphics/pokedex/cat_icon_cancel.gbapal \
	graphics/pokedex/cat_icon_cave.gbapal \
	graphics/pokedex/cat_icon_forest.gbapal \
	graphics/pokedex/cat_icon_grassland.gbapal \
	graphics/pokedex/cat_icon_lightest.gbapal \
	graphics/pokedex/cat_icon_mountain.gbapal \
	graphics/pokedex/cat_icon_numerical.gbapal \
	graphics/pokedex/cat_icon_qmark.gbapal \
	graphics/pokedex/cat_icon_rare.gbapal \
	graphics/pokedex/cat_icon_rough_terrain.gbapal \
	graphics/pokedex/cat_icon_sea.gbapal \
	graphics/pokedex/cat_icon_smallest.gbapal \
	graphics/pokedex/cat_icon_type.gbapal \
	graphics/pokedex/cat_icon_urban.gbapal \
	graphics/pokedex/cat_icon_waters_edge.gbapal \
	graphics/summary_screen/pokerus_cured.gbapal \
	graphics/summary_screen/shiny_star.gbapal

BATTLE_EXTRA_BIN_LZ := \
	graphics/pokemon_special_anim/bg.bin.lz \
	graphics/summary_screen/moves_info_page.bin.lz \
	graphics/summary_screen/moves_page.bin.lz

# Compressed 4bpp assets need an uncompressed intermediate.  The direct list
# contains only the INCBIN targets that are not also intermediates.
BATTLE_EXTRA_4BPP_FROM_PNG := \
	$(BATTLE_EXTRA_DIRECT_4BPP) \
	$(BATTLE_EXTRA_4BPP_LZ:.lz=)

BATTLE_EXTRA_REQUIRED := \
	$(BATTLE_EXTRA_DIRECT_4BPP) \
	$(BATTLE_EXTRA_4BPP_LZ) \
	$(BATTLE_EXTRA_GBAPAL_FROM_PAL) \
	$(BATTLE_EXTRA_GBAPAL_FROM_PNG) \
	$(BATTLE_EXTRA_BIN_LZ)

.PHONY: battle-extra-assets
battle-extra-assets: $(BATTLE_EXTRA_REQUIRED)
	@for asset in $(BATTLE_EXTRA_REQUIRED); do test -e "$$asset" || exit 1; done

$(BATTLE_EXTRA_4BPP_FROM_PNG): %.4bpp: %.png
	@mkdir -p $(dir $@)
	@$(GFX) $< $@

$(BATTLE_EXTRA_GBAPAL_FROM_PAL): %.gbapal: %.pal
	@mkdir -p $(dir $@)
	@$(GFX) $< $@

$(BATTLE_EXTRA_GBAPAL_FROM_PNG): %.gbapal: %.png
	@mkdir -p $(dir $@)
	@$(GFX) $< $@

$(BATTLE_EXTRA_4BPP_LZ): %.4bpp.lz: %.4bpp
	@mkdir -p $(dir $@)
	@$(GFX) $< $@

$(BATTLE_EXTRA_BIN_LZ): %.bin.lz: %.bin
	@mkdir -p $(dir $@)
	@$(GFX) $< $@
