// Hidden-item bg events (AGENTS.md fix #57). 183 of these were emitted as
// ordinary BG events, so A-press did nothing and the A-press branch of
// field_control_avatar.c was dead code.
//
// Pure: no boot needed. The whole defect is in the generated map data and the
// packing helper, both of which are readable from any process.

#include "registry.h"

#include "constants/event_bg.h"
#include "constants/flags.h"
#include "constants/items.h"
#include "field_specials.h"
#include "global.fieldmap.h"

extern const struct MapHeader CeladonCity;
extern const struct MapHeader CeladonCity_GameCorner;

// The packed value stores the flag as an index relative to FLAG_HIDDEN_ITEMS_START;
// GetHiddenItemAttr adds the base back. Storing the absolute id instead points
// ~1000 flags away at an unrelated flag -- which is what a wrong implementation
// does, and what this asserts against.
OMP_TEST("hidden-items/packing round-trips through GetHiddenItemAttr", "hidden-items pure",
         hidden_item_packing)
{
    const struct MapEvents *celadon = CeladonCity.events;
    int i, seen = 0;

    TEST_PTR_NOT_NULL(celadon);

    for (i = 0; i < celadon->bgEventCount; i++)
    {
        const struct BgEvent *bg = &celadon->bgEvents[i];

        if (bg->kind != BG_EVENT_HIDDEN_ITEM)
            continue;
        if (GetHiddenItemAttr(bg->bgUnion.hiddenItem, HIDDEN_ITEM_FLAG)
            != FLAG_HIDDEN_ITEM_CELADON_CITY_PP_UP)
            continue;

        seen = 1;
        TEST_EQ(GetHiddenItemAttr(bg->bgUnion.hiddenItem, HIDDEN_ITEM_ITEM), ITEM_PP_UP);
        TEST_EQ(GetHiddenItemAttr(bg->bgUnion.hiddenItem, HIDDEN_ITEM_QUANTITY), 1);
        TEST_EQ(GetHiddenItemAttr(bg->bgUnion.hiddenItem, HIDDEN_ITEM_UNDERFOOT), 0);
    }

    TEST_EQ(seen, 1);
}

// The Game Corner's buried coins deliberately store ITEM_NONE, which is how the
// script selects its coin branch over the item branch. A 12-entry exception that
// looks like a bug but is not.
OMP_TEST("hidden-items/buried coins store ITEM_NONE on purpose", "hidden-items pure",
         hidden_item_coins)
{
    const struct MapEvents *corner = CeladonCity_GameCorner.events;
    int i, coinSpots = 0;

    TEST_PTR_NOT_NULL(corner);

    for (i = 0; i < corner->bgEventCount; i++)
    {
        const struct BgEvent *bg = &corner->bgEvents[i];

        if (bg->kind == BG_EVENT_HIDDEN_ITEM
            && GetHiddenItemAttr(bg->bgUnion.hiddenItem, HIDDEN_ITEM_ITEM) == ITEM_NONE)
            coinSpots++;
    }

    TEST_GE(coinSpots, 1);
}
