// Save-backed stub guards.
//
// These read live save blocks, which do not exist until SetSaveBlocksPointers
// runs during the boot -- gSaveBlock1Ptr/gSaveBlock2Ptr are NULL before that, so
// unlike the pure map-wiring tests they need an engine fixture.
//
// Each one pins the observable consequence of a stub that was previously
// wrong-typed or hand-mirrored (AGENTS.md fixes #57/#58). A void body or an
// always-0 body passes the compiler and fails here.

#include "registry.h"

#include "coins.h"
#include "constants/pokedex.h"
#include "load_save.h"
#include "pokedex.h"
#include "pokemon.h"

// gPokedexEntries used to be a 1-byte stub indexed as 0x24-byte structs, so every
// read walked off the end of the array. Seen/caught must stick.
FIRERED_TEST("save/pokedex seen and caught flags persist", "engine fixture:bedroom",
         save_pokedex_flags)
{
    TEST_EQ(GetSetPokedexFlag(NATIONAL_DEX_MEWTWO, FLAG_GET_CAUGHT), 0);

    GetSetPokedexFlag(NATIONAL_DEX_MEWTWO, FLAG_SET_SEEN);
    TEST_EQ(GetSetPokedexFlag(NATIONAL_DEX_MEWTWO, FLAG_GET_SEEN), 1);
    TEST_EQ(GetSetPokedexFlag(NATIONAL_DEX_MEWTWO, FLAG_GET_CAUGHT), 0);

    GetSetPokedexFlag(NATIONAL_DEX_MEWTWO, FLAG_SET_CAUGHT);
    TEST_EQ(GetSetPokedexFlag(NATIONAL_DEX_MEWTWO, FLAG_GET_CAUGHT), 1);
}

// AddCoins/RemoveCoins were void stubs, so the Game Corner's "you can't carry any
// more" branch and the Coin Case both read garbage. RemoveCoins must refuse when
// the balance is short, and a refusal must not change the balance.
FIRERED_TEST("save/coins add, remove and refuse to overdraw", "engine fixture:bedroom",
         save_coins)
{
    TEST_EQ(AddCoins(50), TRUE);
    TEST_EQ(GetCoins(), 50);

    TEST_EQ(RemoveCoins(20), TRUE);
    TEST_EQ(GetCoins(), 30);

    TEST_EQ(RemoveCoins(31), FALSE);
    TEST_EQ(GetCoins(), 30);   // the refused removal left the balance alone
}

// The Pokédex flag setter takes a third "is this a species index" parameter and
// both GETS and SETS; a stub that always returned 0 meant the dex never recorded
// anything, Oak's rating was pinned to its lowest tier and the Repeat Ball's
// caught-bonus was dead.
//
// Seen must be recorded alongside caught: the real DexScreen_GetSetPokedexFlag
// treats a caught bit as valid only when it agrees with seen and both of
// SaveBlock1's seen mirrors (its anticheat check). This test used to set only the
// caught bit, which the hand-mirrored stub in overworld_stubs.c counted anyway --
// so it pinned the mirror, not the engine -- and it started failing the moment the
// real src/pokedex_screen.c was linked, exactly as it should have.
FIRERED_TEST("save/pokedex counts reflect caught mons", "engine fixture:bedroom",
         save_pokedex_count)
{
    u16 before = GetNationalPokedexCount(FLAG_GET_CAUGHT);

    HandleSetPokedexFlag(NATIONAL_DEX_ARTICUNO, FLAG_SET_SEEN, 0);
    HandleSetPokedexFlag(NATIONAL_DEX_ARTICUNO, FLAG_SET_CAUGHT, 0);

    TEST_EQ(GetSetPokedexFlag(NATIONAL_DEX_ARTICUNO, FLAG_GET_SEEN), 1);
    TEST_EQ(GetSetPokedexFlag(NATIONAL_DEX_ARTICUNO, FLAG_GET_CAUGHT), 1);
    TEST_EQ(GetNationalPokedexCount(FLAG_GET_CAUGHT), before + 1);
}
