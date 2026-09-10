# FireRed Silicon

An experimental attempt to run the Pokémon FireRed/LeafGreen decompilation natively on Apple Silicon.

This project explores what it takes to adapt a Game Boy Advance codebase to a 64-bit host, including native platform support, SDL2 integration, and portable replacements for selected GBA services.

## Project status

This is very much a work in progress. It is a research and entertainment project, not a finished game or supported product. Functionality is incomplete and may be unstable, and there is no promise that it will ever become complete, polished, or fully playable.

I also cannot promise that I will ever publish prebuilt binaries, release artifacts, or complete compilation instructions. Please treat this repository as an evolving experiment rather than a supported distribution.

## Runtime timing

The native engine targets the GBA's approximately **59.7275 frames per second**
(280,896 CPU cycles per frame at 16,777,216 Hz), independently of monitor refresh
rate. SDL's monotonic performance counter drives absolute frame deadlines;
engine and rendering work count toward that budget. Display VSync is disabled
so a high-refresh display cannot speed up gameplay.

Long stalls resynchronize the clock rather than replaying missed ticks in a
burst. If the host cannot keep up, the game slows down instead of skipping engine
updates. Both `--boot-test N` and the standalone `--test` use the same pacing;
a 9,300-frame boot test takes roughly 156 seconds plus startup overhead.

## Contributions

Contributions and pull requests of all kinds are welcome. Please see [CONTRIBUTING.md](CONTRIBUTING.md) for the current review process.

## What this is—and isn't

- An unofficial, independent native-port experiment for Apple Silicon
- A project built from and inspired by the [pret/pokefirered](https://github.com/pret/pokefirered) decompilation
- Not the official Pokémon FireRed or LeafGreen game
- Not affiliated with, endorsed by, or connected to Nintendo, The Pokémon Company, or Game Freak
- Not a project for distributing the original commercial game or its ROMs

Pokémon, FireRed, LeafGreen, Nintendo, The Pokémon Company, and Game Freak are trademarks of their respective owners. This project is provided for research and entertainment purposes only.
