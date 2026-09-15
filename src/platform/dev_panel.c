// Native developer panel: a separate SDL2 window listing every map the engine
// can load, so a warp destination can be picked by name instead of by
// hand-written coordinates.
//
// Host-side only, in two respects that matter:
//   - The destination table is built ONCE, from Platform_Init before AgbMain
//     starts. Reading gMapGroups and the map layouts is only safe while the
//     engine is stopped: a live map load rewrites gMapHeader, and InitMap (a
//     map-script command) mutates the block data those headers point at.
//   - A warp request is applied at the one place in a frame where the GBA
//     script command `warp` applies one -- SetWarpDestination + DoWarp +
//     ResetInitialPlayerAvatarState -- never from inside the SDL event pump,
//     which runs while the engine is part-way through a frame.
//
// The panel never touches the 240x160 framebuffer or any screenshot path; it
// draws into its own window through its own renderer.
//
// Input: the panel window owns its own keyboard focus, so a headless run (which
// can never focus a window) drives it through --dev-panel-keys, which injects
// one keystroke per frame.

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <SDL.h>

#include "global.h"
#include "global.fieldmap.h"
#include "field_fadetransition.h"
#include "main.h"
#include "overworld.h"
#include "constants/maps.h"
#include "platform/platform.h"

bool gPlatformDevPanelEnabled = false;

static const char *sKeySequence = NULL;
const char *gPlatformDevPanelKeySequence = NULL;

// Lazy tokeniser state, one token consumed per frame. Keeping the cursor and
// the repeat count apart from gPlatformDevPanelKeySequence leaves that pointer
// as the caller passed it (argc argv storage, never mutated).
static int sCurrentKey;
static int sKeyRepeats;
static bool sSequenceStarted;

// Emitted by tools/gen_map_data.py, next to the tables they describe. The map
// layout structs carry no names, so the names come from the generator's JSON.
extern const struct MapHeader *const *const gMapGroups[];
extern const u8 gMapGroupCounts[];
extern const char *const gMapGroupNames[];
extern const u16 gMapGroupFirstMap[];
extern const char *const gMapNames[];

#define DEV_PANEL_WIDTH        400
#define DEV_PANEL_HEIGHT       256
#define DEV_PANEL_MARGIN       8

#define DEV_PANEL_FONT_W       5
#define DEV_PANEL_FONT_H       7
#define DEV_PANEL_FONT_SPACING 1
#define DEV_PANEL_ADVANCE      (DEV_PANEL_FONT_W + DEV_PANEL_FONT_SPACING)

#define DEV_PANEL_LIST_ROWS    12
#define DEV_PANEL_LIST_ROW_H   10
#define DEV_PANEL_LIST_X       DEV_PANEL_MARGIN
#define DEV_PANEL_LIST_Y       44
#define DEV_PANEL_LIST_W       (DEV_PANEL_WIDTH - 2 * DEV_PANEL_MARGIN)
#define DEV_PANEL_LIST_H       (DEV_PANEL_LIST_ROWS * DEV_PANEL_LIST_ROW_H + 4)

// Rows are one per warp event, plus one walk-in row for a map with no warp
// events at all. FireRed has 425 maps and 1294 warp events, so this has room to
// spare; BuildDestinations() reports an overflow instead of truncating silently
// if a future map set outgrows it.
#define DEV_PANEL_MAX_ROWS     2048

#define DEV_PANEL_FILTER_LEN   32
#define DEV_PANEL_TEXT_LEN     40
#define DEV_PANEL_TEXT_ROWS    2

// Focus targets for the keyboard. The panel owns the arrow keys unless one of
// the text fields has focus.
enum
{
    DEV_PANEL_FOCUS_LIST,
    DEV_PANEL_FOCUS_FILTER,
    DEV_PANEL_FOCUS_TEXT1,
    DEV_PANEL_FOCUS_TEXT2,
};

struct DevDestination
{
    u8 group;
    u8 num;
    s8 warpId;  // WARP_ID_NONE for a map that has no warp event to inherit
    s16 x;
    s16 y;
};

static struct DevDestination sTable[DEV_PANEL_MAX_ROWS];
static int sCount;

static int *sMatches;
static int sMatchCount;
static int sSelected;
static int sScroll;

static char sFilter[DEV_PANEL_FILTER_LEN];
static char sTextFields[DEV_PANEL_TEXT_ROWS][DEV_PANEL_TEXT_LEN];
static int sTextScroll[DEV_PANEL_TEXT_ROWS];
static int sFocus = DEV_PANEL_FOCUS_LIST;
static char sStatus[80];
static int sStatusCountdown;

// Warp request, applied in Platform_DevPanelUpdate after the frame's script
// context has already run.
static bool sWarpRequested;
static s8 sQueuedWarpGroup;
static s8 sQueuedWarpNum;
static s8 sQueuedWarpId;
static s16 sQueuedWarpX;
static s16 sQueuedWarpY;

static SDL_Window *sPanelWindow = NULL;
static SDL_Renderer *sPanelRenderer = NULL;
static SDL_Texture *sPanelTexture = NULL;
static u32 *sPanelPixels = NULL;
static bool sPanelDirty = true;

static void EnsureTables(void);

// Panel palette, ARGB8888. These are the colours themselves, not indices into
// a table: a name that has to be looked up in a second declaration is one more
// place for the two to disagree.
#define COL_BG        0xFF101418
#define COL_TEXT      0xFFD8DEE6
#define COL_DIM       0xFF8A939E
#define COL_FRAME     0xFF3C4652
#define COL_FIELD     0xFF0A0D10
#define COL_SEL       0xFF2F6FD0
#define COL_SEL_TEXT  0xFFFFFFFF
#define COL_CURSOR    0xFF7FD4FF
#define COL_GOOD      0xFF7FCF6A

// 5x7 glyphs for ASCII 32..126, one entry per printable character. Bit 4 is the
// leftmost column, matching the orientation SDL draws them in.
static const u8 sGlyphs[95][DEV_PANEL_FONT_H] =
{
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // ' '
    { 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04 }, // '!'
    { 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00 }, // '"'
    { 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x00, 0x00 }, // '#'
    { 0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04 }, // '$'
    { 0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03 }, // '%'
    { 0x0C, 0x12, 0x12, 0x0C, 0x15, 0x12, 0x0D }, // '&'
    { 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00 }, // '\''
    { 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02 }, // '('
    { 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08 }, // ')'
    { 0x00, 0x04, 0x15, 0x0E, 0x15, 0x04, 0x00 }, // '*'
    { 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00 }, // '+'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x08 }, // ','
    { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 }, // '-'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C }, // '.'
    { 0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10 }, // '/'
    { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E }, // '0'
    { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E }, // '1'
    { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F }, // '2'
    { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E }, // '3'
    { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 }, // '4'
    { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E }, // '5'
    { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E }, // '6'
    { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 }, // '7'
    { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E }, // '8'
    { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C }, // '9'
    { 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00 }, // ':'
    { 0x00, 0x00, 0x0C, 0x00, 0x0C, 0x08, 0x00 }, // ';'
    { 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02 }, // '<'
    { 0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00 }, // '='
    { 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08 }, // '>'
    { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04 }, // '?'
    { 0x0E, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0E }, // '@'
    { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 }, // 'A'
    { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E }, // 'B'
    { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E }, // 'C'
    { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E }, // 'D'
    { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F }, // 'E'
    { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 }, // 'F'
    { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F }, // 'G'
    { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 }, // 'H'
    { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E }, // 'I'
    { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C }, // 'J'
    { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 }, // 'K'
    { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F }, // 'L'
    { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 }, // 'M'
    { 0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11 }, // 'N'
    { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E }, // 'O'
    { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 }, // 'P'
    { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D }, // 'Q'
    { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 }, // 'R'
    { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E }, // 'S'
    { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 }, // 'T'
    { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E }, // 'U'
    { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 }, // 'V'
    { 0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11 }, // 'W'
    { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 }, // 'X'
    { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 }, // 'Y'
    { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F }, // 'Z'
    { 0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E }, // '['
    { 0x10, 0x08, 0x08, 0x04, 0x02, 0x02, 0x01 }, // '\\'
    { 0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E }, // ']'
    { 0x04, 0x0A, 0x11, 0x00, 0x00, 0x00, 0x00 }, // '^'
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F }, // '_'
    { 0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00 }, // '`'
    { 0x00, 0x00, 0x0E, 0x01, 0x0F, 0x11, 0x0F }, // 'a'
    { 0x10, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x1E }, // 'b'
    { 0x00, 0x00, 0x0E, 0x11, 0x10, 0x11, 0x0E }, // 'c'
    { 0x01, 0x01, 0x0F, 0x11, 0x11, 0x11, 0x0F }, // 'd'
    { 0x00, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E }, // 'e'
    { 0x06, 0x09, 0x08, 0x1C, 0x08, 0x08, 0x08 }, // 'f'
    { 0x00, 0x0F, 0x11, 0x11, 0x0F, 0x01, 0x0E }, // 'g'
    { 0x10, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x11 }, // 'h'
    { 0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x0E }, // 'i'
    { 0x02, 0x00, 0x06, 0x02, 0x02, 0x12, 0x0C }, // 'j'
    { 0x10, 0x10, 0x12, 0x14, 0x18, 0x14, 0x12 }, // 'k'
    { 0x0C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E }, // 'l'
    { 0x00, 0x00, 0x1B, 0x15, 0x15, 0x15, 0x15 }, // 'm'
    { 0x00, 0x00, 0x1E, 0x11, 0x11, 0x11, 0x11 }, // 'n'
    { 0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E }, // 'o'
    { 0x00, 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10 }, // 'p'
    { 0x00, 0x0F, 0x11, 0x11, 0x0F, 0x01, 0x01 }, // 'q'
    { 0x00, 0x00, 0x16, 0x19, 0x10, 0x10, 0x10 }, // 'r'
    { 0x00, 0x00, 0x0F, 0x10, 0x0E, 0x01, 0x1E }, // 's'
    { 0x08, 0x08, 0x1C, 0x08, 0x08, 0x09, 0x06 }, // 't'
    { 0x00, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0D }, // 'u'
    { 0x00, 0x00, 0x11, 0x11, 0x11, 0x0A, 0x04 }, // 'v'
    { 0x00, 0x00, 0x11, 0x15, 0x15, 0x15, 0x0A }, // 'w'
    { 0x00, 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11 }, // 'x'
    { 0x00, 0x11, 0x11, 0x11, 0x0F, 0x01, 0x0E }, // 'y'
    { 0x00, 0x00, 0x1F, 0x02, 0x04, 0x08, 0x1F }, // 'z'
    { 0x02, 0x04, 0x04, 0x08, 0x04, 0x04, 0x02 }, // '{'
    { 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 }, // '|'
    { 0x08, 0x04, 0x04, 0x02, 0x04, 0x04, 0x08 }, // '}'
    { 0x00, 0x00, 0x08, 0x15, 0x02, 0x00, 0x00 }, // '~'
};

// A short glyph table is a silent failure: the missing entries are zero-filled,
// which is indistinguishable from a space, so text renders blank with no
// compiler complaint. Pin the count. (The table must also stay in ASCII order
// from 32; DrawGlyph indexes it as sGlyphs[ch - 32].)
STATIC_ASSERT(ARRAY_COUNT(sGlyphs) == 126 - 32 + 1, DevPanelGlyphTableCoversPrintableAscii);

// ---------------------------------------------------------------------------
// Scripted keys (--dev-panel-keys)
// ---------------------------------------------------------------------------
//
// The panel window owns its own keyboard focus, so a headless run cannot drive
// it through SDL at all. The sequence below is tokenised lazily, one token per
// frame, so a token like WAIT*120 costs nothing to skip.

static int TokenKey(const char *token)
{
    if (strcmp(token, "UP") == 0)       return SDLK_UP;
    if (strcmp(token, "DOWN") == 0)     return SDLK_DOWN;
    if (strcmp(token, "LEFT") == 0)     return SDLK_LEFT;
    if (strcmp(token, "RIGHT") == 0)    return SDLK_RIGHT;
    if (strcmp(token, "PGUP") == 0)     return SDLK_PAGEUP;
    if (strcmp(token, "PGDN") == 0)     return SDLK_PAGEDOWN;
    if (strcmp(token, "HOME") == 0)     return SDLK_HOME;
    if (strcmp(token, "END") == 0)      return SDLK_END;
    if (strcmp(token, "ENTER") == 0)    return SDLK_RETURN;
    if (strcmp(token, "TAB") == 0)      return SDLK_TAB;
    if (strcmp(token, "ESC") == 0)      return SDLK_ESCAPE;
    if (strcmp(token, "BACKSPACE") == 0)return SDLK_BACKSPACE;
    if (strcmp(token, "F3") == 0)       return SDLK_F3;
    if (strcmp(token, "WAIT") == 0)     return 0;
    if (strcmp(token, "SPACE") == 0)    return ' ';
    if (token[0] != '\0' && token[1] == '\0')
        return (unsigned char)token[0]; // a single character, used verbatim

    return 0;
}

// Advances to the next token, setting sCurrentKey and sKeyRepeats. Returns
// false once the sequence is exhausted.
static bool NextSequenceKey(void)
{
    const char *end;
    char token[16];
    int length;
    s32 repeats = 1;

    if (sKeySequence == NULL)
        return false;

    while (*sKeySequence == ',' || *sKeySequence == ' ')
        sKeySequence++;

    if (*sKeySequence == '\0')
        return false;

    end = sKeySequence;
    while (*end != '\0' && *end != ',' && *end != '*' && *end != ' ')
        end++;

    length = (int)(end - sKeySequence);
    if (length >= (int)sizeof(token))
        length = (int)sizeof(token) - 1;
    memcpy(token, sKeySequence, length);
    token[length] = '\0';
    sKeySequence = end;

    if (*sKeySequence == '*')
    {
        repeats = atoi(sKeySequence + 1);
        if (repeats < 1)
            repeats = 1;
        while (*sKeySequence != '\0' && *sKeySequence != ',')
            sKeySequence++;
    }

    sCurrentKey = TokenKey(token);
    sKeyRepeats = repeats;
    return true;
}

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

static void SetStatus(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vsnprintf(sStatus, sizeof(sStatus), fmt, args);
    va_end(args);
    sStatusCountdown = 300;
    sPanelDirty = true;
}

static int StringLength(const char *str)
{
    int i = 0;
    while (str[i] != '\0')
        i++;
    return i;
}

static const char *DestinationName(const struct DevDestination *dest)
{
    return gMapNames[gMapGroupFirstMap[dest->group] + dest->num];
}

// ---------------------------------------------------------------------------
// Destination table
// ---------------------------------------------------------------------------

static bool IsBlockWalkable(const struct MapHeader *header, s32 x, s32 y)
{
    const struct MapLayout *layout = header->mapLayout;
    u16 block;

    if (x < 0 || y < 0 || x >= layout->width || y >= layout->height)
        return false;

    block = layout->map[y * layout->width + x];
    if (block == MAPGRID_UNDEFINED)
        return false;

    return ((block & MAPGRID_COLLISION_MASK) >> MAPGRID_COLLISION_SHIFT) == 0;
}

// Some maps (the Hall of Fame, the Cable Club rooms, a few test maps) have no
// warp events, so there is no coordinate to inherit. Prefer the map's centre if
// it is walkable and otherwise spiral outwards for the nearest walkable block,
// which is a far better guess than (0,0) -- (0,0) of a 10x9 indoor map is its
// top-left wall.
static void FindWalkableCoords(const struct MapHeader *header, s16 *outX, s16 *outY)
{
    s32 width = header->mapLayout->width;
    s32 height = header->mapLayout->height;
    s32 centreX = width / 2;
    s32 centreY = height / 2;
    s32 radius;
    s32 maxRadius = (width > height ? width : height) / 2 + 1;

    if (IsBlockWalkable(header, centreX, centreY))
    {
        *outX = centreX;
        *outY = centreY;
        return;
    }

    for (radius = 1; radius <= maxRadius; radius++)
    {
        s32 x, y;

        for (y = centreY - radius; y <= centreY + radius; y++)
        {
            for (x = centreX - radius; x <= centreX + radius; x++)
            {
                // Only the ring at this radius; the inside was already searched.
                if (abs(x - centreX) != radius && abs(y - centreY) != radius)
                    continue;
                if (IsBlockWalkable(header, x, y))
                {
                    *outX = x;
                    *outY = y;
                    return;
                }
            }
        }
    }

    *outX = centreX;
    *outY = centreY;
}

static void BuildDestinations(void)
{
    u32 group;

    sCount = 0;

    for (group = 0; group < MAP_GROUPS_COUNT; group++)
    {
        u32 mapNum;

        for (mapNum = 0; mapNum < gMapGroupCounts[group]; mapNum++)
        {
            const struct MapHeader *header = gMapGroups[group][mapNum];
            const struct MapEvents *events = header->events;
            u32 rows = (events != NULL && events->warpCount > 0) ? events->warpCount : 1;
            u32 i;

            for (i = 0; i < rows; i++)
            {
                struct DevDestination *dest;

                if (sCount >= DEV_PANEL_MAX_ROWS)
                {
                    fprintf(stderr, "[DevPanel] destination table full at %s\n",
                            gMapNames[gMapGroupFirstMap[group] + mapNum]);
                    return;
                }

                dest = &sTable[sCount];
                dest->group = group;
                dest->num = mapNum;

                if (events != NULL && events->warpCount > 0)
                {
                    dest->warpId = i;
                    dest->x = events->warps[i].x;
                    dest->y = events->warps[i].y;
                }
                else
                {
                    dest->warpId = WARP_ID_NONE;
                    FindWalkableCoords(header, &dest->x, &dest->y);
                }

                // SetWarpDestination takes s8 coordinates, as pret's does.
                if (dest->x < 0)
                    dest->x = 0;
                if (dest->x > 127)
                    dest->x = 127;
                if (dest->y < 0)
                    dest->y = 0;
                if (dest->y > 127)
                    dest->y = 127;

                sCount++;
            }
        }
    }

    printf("[DevPanel] %d warp destinations across %d maps\n", sCount, (int)MAP_GROUPS_COUNT);
}

// ---------------------------------------------------------------------------
// Filtering and selection
// ---------------------------------------------------------------------------

static bool ContainsIgnoreCase(const char *haystack, const char *needle)
{
    s32 i, j;

    if (needle[0] == '\0')
        return true;

    for (i = 0; haystack[i] != '\0'; i++)
    {
        for (j = 0; needle[j] != '\0'; j++)
        {
            char a = haystack[i + j];
            char b = needle[j];

            if (a >= 'A' && a <= 'Z')
                a += 'a' - 'A';
            if (b >= 'A' && b <= 'Z')
                b += 'a' - 'A';
            if (a != b)
                break;
        }
        if (needle[j] == '\0')
            return true;
    }

    return false;
}

static void RebuildMatches(void)
{
    int i;

    sMatchCount = 0;
    for (i = 0; i < sCount; i++)
    {
        if (ContainsIgnoreCase(DestinationName(&sTable[i]), sFilter))
            sMatches[sMatchCount++] = i;
    }

    sSelected = 0;
    sScroll = 0;
    sPanelDirty = true;
}

static void ClampSelection(void)
{
    if (sMatchCount == 0)
    {
        sSelected = 0;
        sScroll = 0;
        return;
    }

    if (sSelected < 0)
        sSelected = 0;
    if (sSelected > sMatchCount - 1)
        sSelected = sMatchCount - 1;

    if (sSelected < sScroll)
        sScroll = sSelected;
    if (sSelected >= sScroll + DEV_PANEL_LIST_ROWS)
        sScroll = sSelected - DEV_PANEL_LIST_ROWS + 1;
}

static void MoveSelection(int delta)
{
    if (sMatchCount == 0)
        return;

    sSelected += delta;
    ClampSelection();
    sPanelDirty = true;
}

// ---------------------------------------------------------------------------
// Warp request
// ---------------------------------------------------------------------------

static void RequestWarp(void)
{
    const struct DevDestination *dest;

    if (sMatchCount == 0)
    {
        SetStatus("no destination selected");
        return;
    }

    dest = &sTable[sMatches[sSelected]];
    sQueuedWarpGroup = dest->group;
    sQueuedWarpNum = dest->num;
    sQueuedWarpId = dest->warpId;
    sQueuedWarpX = dest->x;
    sQueuedWarpY = dest->y;
    sWarpRequested = true;
}

// gMapHeader is only rebuilt while the engine runs a map load, so the panel
// refuses to warp anywhere the load loop would have to be interrupted: during a
// battle, or while a scene other than the overworld owns callback1.
static bool IsOverworldActive(void)
{
    return gMain.callback1 == CB1_Overworld && !gMain.inBattle;
}

static void ApplyQueuedWarp(void)
{
    sWarpRequested = false;

    if (!IsOverworldActive())
    {
        SetStatus("cannot warp: overworld is not running");
        return;
    }

    // Exactly what ScrCmd_warp does, so the warp goes through the engine's own
    // fade, map load and field-callback hand-off rather than a private path.
    SetWarpDestination(sQueuedWarpGroup, sQueuedWarpNum, sQueuedWarpId, sQueuedWarpX, sQueuedWarpY);
    DoWarp();
    ResetInitialPlayerAvatarState();

    printf("[DevPanel] warp -> %s %u/%u (%d,%d)\n",
           gMapNames[gMapGroupFirstMap[(u32)sQueuedWarpGroup] + (u32)sQueuedWarpNum],
           sQueuedWarpNum, sQueuedWarpGroup, (int)sQueuedWarpX, (int)sQueuedWarpY);

    SetStatus("warp requested: %s (%d,%d)",
              gMapNames[gMapGroupFirstMap[(u32)sQueuedWarpGroup] + (u32)sQueuedWarpNum],
              (int)sQueuedWarpX, (int)sQueuedWarpY);
}

// ---------------------------------------------------------------------------
// Text fields
// ---------------------------------------------------------------------------

static char *ActiveTextField(void)
{
    if (sFocus == DEV_PANEL_FOCUS_FILTER)
        return sFilter;
    if (sFocus == DEV_PANEL_FOCUS_TEXT1)
        return sTextFields[0];
    if (sFocus == DEV_PANEL_FOCUS_TEXT2)
        return sTextFields[1];
    return NULL;
}

static int ActiveTextLimit(void)
{
    return sFocus == DEV_PANEL_FOCUS_FILTER ? DEV_PANEL_FILTER_LEN : DEV_PANEL_TEXT_LEN;
}

static int *ActiveTextScroll(void)
{
    if (sFocus == DEV_PANEL_FOCUS_TEXT1)
        return &sTextScroll[0];
    if (sFocus == DEV_PANEL_FOCUS_TEXT2)
        return &sTextScroll[1];
    return NULL;
}

static void TextInsert(char ch)
{
    char *field = ActiveTextField();
    int limit = ActiveTextLimit();
    int len;

    if (field == NULL || ch < 32 || ch > 126)
        return;

    len = StringLength(field);
    if (len >= limit - 1)
        return;

    field[len] = ch;
    field[len + 1] = '\0';

    if (sFocus == DEV_PANEL_FOCUS_FILTER)
        RebuildMatches();
    sPanelDirty = true;
}

static void TextBackspace(void)
{
    char *field = ActiveTextField();
    int len;

    if (field == NULL)
        return;

    len = StringLength(field);
    if (len > 0)
        field[len - 1] = '\0';

    if (sFocus == DEV_PANEL_FOCUS_FILTER)
        RebuildMatches();
    sPanelDirty = true;
}

static void CycleFocus(void)
{
    sFocus = (sFocus + 1) % (DEV_PANEL_TEXT_ROWS + 2);
    sPanelDirty = true;
}

static void CopySelectionToText(int row)
{
    const struct DevDestination *dest;
    char buffer[DEV_PANEL_TEXT_LEN];

    if (sMatchCount == 0 || row < 0 || row >= DEV_PANEL_TEXT_ROWS)
        return;

    dest = &sTable[sMatches[sSelected]];
    if (row == 0)
        snprintf(buffer, sizeof(buffer), "%s", DestinationName(dest));
    else
        snprintf(buffer, sizeof(buffer), "%s %u/%u %d,%d",
                 gMapGroupNames[dest->group], dest->num, dest->group,
                 (int)dest->x, (int)dest->y);

    snprintf(sTextFields[row], DEV_PANEL_TEXT_LEN, "%s", buffer);
    sTextScroll[row] = 0;
    SetStatus("copied to text field %d", row + 1);
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

static void FillRect(int x, int y, int w, int h, u32 color)
{
    int px, py;

    for (py = y; py < y + h; py++)
    {
        if (py < 0 || py >= DEV_PANEL_HEIGHT)
            continue;
        for (px = x; px < x + w; px++)
        {
            if (px < 0 || px >= DEV_PANEL_WIDTH)
                continue;
            sPanelPixels[py * DEV_PANEL_WIDTH + px] = color;
        }
    }
}

static void DrawFrame(int x, int y, int w, int h, u32 color)
{
    FillRect(x, y, w, 1, color);
    FillRect(x, y + h - 1, w, 1, color);
    FillRect(x, y, 1, h, color);
    FillRect(x + w - 1, y, 1, h, color);
}

static void DrawGlyph(int x, int y, char ch, u32 color)
{
    const u8 *rows;
    int row, column;

    if (ch < 32 || ch > 126)
        ch = '?';
    rows = sGlyphs[(int)ch - 32];

    for (row = 0; row < DEV_PANEL_FONT_H; row++)
    {
        for (column = 0; column < DEV_PANEL_FONT_W; column++)
        {
            if (!(rows[row] & (1 << (DEV_PANEL_FONT_W - 1 - column))))
                continue;
            FillRect(x + column, y + row, 1, 1, color);
        }
    }
}

// Draws at most maxChars glyphs; anything longer is dropped rather than
// overdrawing whatever sits next to it.
static void DrawText(int x, int y, const char *text, u32 color, int maxChars)
{
    int i;

    for (i = 0; text[i] != '\0' && i < maxChars; i++)
        DrawGlyph(x + i * DEV_PANEL_ADVANCE, y, text[i], color);
}

static void DrawTextField(int x, int y, int width, const char *value, int scroll, bool focused)
{
    int capacity = width / DEV_PANEL_ADVANCE;
    int length = StringLength(value);

    FillRect(x, y, width, DEV_PANEL_FONT_H + 4, COL_FIELD);
    DrawFrame(x, y, width, DEV_PANEL_FONT_H + 4, focused ? COL_CURSOR : COL_FRAME);

    // Keep the caret in view for values longer than the field, which TEXT 2 can
    // reach on a long group name.
    if (scroll > length)
        scroll = length;
    if (length - scroll > capacity - 1)
        scroll = length - (capacity - 1);

    DrawText(x + 3, y + 2, value + scroll, COL_TEXT, capacity - 1);
    if (focused)
    {
        int caret = 3 + (length - scroll) * DEV_PANEL_ADVANCE;
        if (caret > width - 4)
            caret = width - 4;
        FillRect(x + caret, y + 2, 1, DEV_PANEL_FONT_H, COL_CURSOR);
    }
}

static void RenderPanel(void)
{
    int row;
    int maxChars = (DEV_PANEL_LIST_W - 8) / DEV_PANEL_ADVANCE;

    if (sPanelRenderer == NULL || sPanelTexture == NULL)
        return;

    FillRect(0, 0, DEV_PANEL_WIDTH, DEV_PANEL_HEIGHT, COL_BG);

    DrawText(DEV_PANEL_MARGIN, 6, "FIRERED DEV PANEL - WARP", COL_TEXT, 40);
    DrawText(DEV_PANEL_MARGIN, 16,
             "F3 toggle  UP/DOWN row  PGUP/PGDN page  ENTER warp  TAB field  ESC back",
             COL_DIM, 80);

    DrawText(DEV_PANEL_MARGIN, 30, "FILTER", COL_DIM, 6);
    DrawTextField(DEV_PANEL_MARGIN + 44, 28, DEV_PANEL_WIDTH - 2 * DEV_PANEL_MARGIN - 44,
                  sFilter, 0, sFocus == DEV_PANEL_FOCUS_FILTER);

    DrawFrame(DEV_PANEL_LIST_X, DEV_PANEL_LIST_Y, DEV_PANEL_LIST_W, DEV_PANEL_LIST_H, COL_FRAME);

    for (row = 0; row < DEV_PANEL_LIST_ROWS; row++)
    {
        int matchIndex = sScroll + row;
        int y = DEV_PANEL_LIST_Y + 2 + row * DEV_PANEL_LIST_ROW_H;
        const struct DevDestination *dest;
        bool selected;
        char line[96];

        if (matchIndex >= sMatchCount)
            break;

        dest = &sTable[sMatches[matchIndex]];
        selected = (matchIndex == sSelected);
        if (selected)
            FillRect(DEV_PANEL_LIST_X + 2, y - 1, DEV_PANEL_LIST_W - 4, DEV_PANEL_LIST_ROW_H, COL_SEL);

        snprintf(line, sizeof(line), "%-18s %-14s %3d,%3d",
                 gMapGroupNames[dest->group], DestinationName(dest),
                 (int)dest->x, (int)dest->y);
        DrawText(DEV_PANEL_LIST_X + 4, y, line, selected ? COL_SEL_TEXT : COL_TEXT, maxChars);
    }

    {
        char line[96];

        snprintf(line, sizeof(line), "%d of %d destinations%s%s",
                 sMatchCount, sCount,
                 sFilter[0] != '\0' ? " matching " : "", sFilter);
        DrawText(DEV_PANEL_MARGIN, DEV_PANEL_LIST_Y + DEV_PANEL_LIST_H + 4, line, COL_DIM, 60);
    }

    if (sMatchCount > 0)
    {
        const struct DevDestination *dest = &sTable[sMatches[sSelected]];
        char line[96];

        if (dest->warpId == WARP_ID_NONE)
            snprintf(line, sizeof(line), "NO WARP EVENT - walking in at %d,%d",
                     (int)dest->x, (int)dest->y);
        else
            snprintf(line, sizeof(line), "WARP EVENT %d at %d,%d in %s",
                     dest->warpId, (int)dest->x, (int)dest->y, gMapGroupNames[dest->group]);
        DrawText(DEV_PANEL_MARGIN, 182, line, COL_DIM, 64);
    }

    DrawText(DEV_PANEL_MARGIN, 194, "TEXT 1", COL_DIM, 6);
    DrawTextField(DEV_PANEL_MARGIN + 44, 192, DEV_PANEL_WIDTH - 2 * DEV_PANEL_MARGIN - 44,
                  sTextFields[0], sTextScroll[0], sFocus == DEV_PANEL_FOCUS_TEXT1);

    DrawText(DEV_PANEL_MARGIN, 210, "TEXT 2", COL_DIM, 6);
    DrawTextField(DEV_PANEL_MARGIN + 44, 208, DEV_PANEL_WIDTH - 2 * DEV_PANEL_MARGIN - 44,
                  sTextFields[1], sTextScroll[1], sFocus == DEV_PANEL_FOCUS_TEXT2);

    DrawText(DEV_PANEL_MARGIN, 222,
             "1 copy map name   2 copy group and index   (list focus)", COL_DIM, 72);

    DrawText(DEV_PANEL_MARGIN, 238, sStatus, COL_GOOD, 76);

    SDL_UpdateTexture(sPanelTexture, NULL, sPanelPixels, DEV_PANEL_WIDTH * sizeof(u32));
    SDL_RenderClear(sPanelRenderer);
    SDL_RenderCopy(sPanelRenderer, sPanelTexture, NULL, NULL);
    SDL_RenderPresent(sPanelRenderer);
    sPanelDirty = false;
}

// ---------------------------------------------------------------------------
// Window lifecycle
// ---------------------------------------------------------------------------

static void CreatePanelWindow(void)
{
    if (sPanelWindow != NULL)
        return;

    sPanelWindow = SDL_CreateWindow("FireRed Dev Panel - Warp",
                                    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    DEV_PANEL_WIDTH, DEV_PANEL_HEIGHT,
                                    SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (sPanelWindow == NULL)
    {
        fprintf(stderr, "[DevPanel] failed to create window: %s\n", SDL_GetError());
        return;
    }

    sPanelRenderer = SDL_CreateRenderer(sPanelWindow, -1, SDL_RENDERER_ACCELERATED);
    if (sPanelRenderer == NULL)
        sPanelRenderer = SDL_CreateRenderer(sPanelWindow, -1, 0);
    if (sPanelRenderer == NULL)
    {
        fprintf(stderr, "[DevPanel] failed to create renderer: %s\n", SDL_GetError());
        return;
    }

    // One panel pixel per logical pixel, so the 5x7 glyphs stay crisp on a
    // high-DPI display.
    SDL_RenderSetLogicalSize(sPanelRenderer, DEV_PANEL_WIDTH, DEV_PANEL_HEIGHT);

    sPanelTexture = SDL_CreateTexture(sPanelRenderer, SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_STREAMING,
                                      DEV_PANEL_WIDTH, DEV_PANEL_HEIGHT);
    if (sPanelTexture == NULL)
    {
        fprintf(stderr, "[DevPanel] failed to create texture: %s\n", SDL_GetError());
        return;
    }

    sPanelPixels = malloc(DEV_PANEL_WIDTH * DEV_PANEL_HEIGHT * sizeof(u32));
    sPanelDirty = true;
}

static void DestroyPanelWindow(void)
{
    if (sPanelPixels != NULL)
    {
        free(sPanelPixels);
        sPanelPixels = NULL;
    }
    if (sPanelTexture != NULL)
    {
        SDL_DestroyTexture(sPanelTexture);
        sPanelTexture = NULL;
    }
    if (sPanelRenderer != NULL)
    {
        SDL_DestroyRenderer(sPanelRenderer);
        sPanelRenderer = NULL;
    }
    if (sPanelWindow != NULL)
    {
        SDL_DestroyWindow(sPanelWindow);
        sPanelWindow = NULL;
    }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

static void HandlePanelKey(int key)
{
    if (ActiveTextField() != NULL)
    {
        int *scroll = ActiveTextScroll();

        switch (key)
        {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            RequestWarp();
            return;
        case SDLK_TAB:
            CycleFocus();
            return;
        case SDLK_BACKSPACE:
            TextBackspace();
            return;
        case SDLK_ESCAPE:
            ActiveTextField()[0] = '\0';
            if (scroll != NULL)
                *scroll = 0;
            if (sFocus == DEV_PANEL_FOCUS_FILTER)
                RebuildMatches();
            sFocus = DEV_PANEL_FOCUS_LIST;
            sPanelDirty = true;
            return;
        default:
            if (key >= 32 && key <= 126)
                TextInsert((char)key);
            return;
        }
    }

    switch (key)
    {
    case SDLK_UP:
        MoveSelection(-1);
        break;
    case SDLK_DOWN:
        MoveSelection(1);
        break;
    case SDLK_PAGEUP:
        MoveSelection(-DEV_PANEL_LIST_ROWS);
        break;
    case SDLK_PAGEDOWN:
        MoveSelection(DEV_PANEL_LIST_ROWS);
        break;
    case SDLK_HOME:
        sSelected = 0;
        ClampSelection();
        sPanelDirty = true;
        break;
    case SDLK_END:
        sSelected = sMatchCount - 1;
        ClampSelection();
        sPanelDirty = true;
        break;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        RequestWarp();
        break;
    case SDLK_TAB:
        CycleFocus();
        break;
    case SDLK_ESCAPE:
        // Clearing the filter is the useful "back out" from the list; F3 closes.
        sFilter[0] = '\0';
        RebuildMatches();
        break;
    case SDLK_1:
        CopySelectionToText(0);
        break;
    case SDLK_2:
        CopySelectionToText(1);
        break;
    default:
        break;
    }
}

bool Platform_DevPanelHandleEvent(void *sdlEvent)
{
    SDL_Event *event = (SDL_Event *)sdlEvent;
    u32 panelWindowId;

    if (!gPlatformDevPanelEnabled || sPanelWindow == NULL)
        return false;

    panelWindowId = SDL_GetWindowID(sPanelWindow);

    if (event->type == SDL_KEYDOWN && event->key.windowID == panelWindowId)
    {
        HandlePanelKey(event->key.keysym.sym);
        return true;
    }

    if (event->type == SDL_WINDOWEVENT && event->window.windowID == panelWindowId)
    {
        if (event->window.event == SDL_WINDOWEVENT_CLOSE)
            DestroyPanelWindow();
        else if (event->window.event == SDL_WINDOWEVENT_EXPOSED)
            sPanelDirty = true;
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Per-frame
// ---------------------------------------------------------------------------

// Reports every map change while the panel is up. The warp request above only
// proves the request; this proves the engine actually landed, which is what a
// headless run needs to check without reading pixels.
static void ReportMapChange(void)
{
    static s8 lastGroup = -1;
    static s8 lastNum = -1;

    if (gSaveBlock1Ptr->location.mapGroup != lastGroup || gSaveBlock1Ptr->location.mapNum != lastNum)
    {
        lastGroup = gSaveBlock1Ptr->location.mapGroup;
        lastNum = gSaveBlock1Ptr->location.mapNum;

        if (lastGroup >= 0 && lastGroup < (s8)MAP_GROUPS_COUNT
         && lastNum >= 0 && lastNum < (s8)gMapGroupCounts[(u32)lastGroup])
        {
            printf("[DevPanel] now in %s %d/%d at (%d,%d)\n",
                   gMapNames[gMapGroupFirstMap[(u32)lastGroup] + (u32)lastNum],
                   lastNum, lastGroup,
                   gSaveBlock1Ptr->pos.x, gSaveBlock1Ptr->pos.y);
        }
        sPanelDirty = true;
    }
}

void Platform_DevPanelUpdate(void)
{
    int scriptedKey = 0;

    // Scripted keys (--dev-panel-keys) are panel input, so they are advanced
    // even while the panel is off: a scripted F3 then toggles it on, exactly as
    // the real key does. A headless run can never deliver that SDL event. The
    // cursor starts here rather than in EnsureTables, which only runs once the
    // panel is already on -- a scripted F3 would otherwise never be consumed.
    if (gPlatformDevPanelKeySequence != NULL && !sSequenceStarted)
    {
        sKeySequence = gPlatformDevPanelKeySequence;
        sKeyRepeats = 0;
        sSequenceStarted = true;
    }

    if (gPlatformDevPanelKeySequence != NULL)
    {
        if (sKeyRepeats <= 0 && !NextSequenceKey())
        {
            gPlatformDevPanelKeySequence = NULL;
            sCurrentKey = 0;
        }
        else
        {
            sKeyRepeats--;
            scriptedKey = sCurrentKey;
        }
    }

    if (scriptedKey == SDLK_F3)
    {
        Platform_DevPanelToggleKey(SDLK_F3);
        scriptedKey = 0;
    }

    if (!gPlatformDevPanelEnabled)
    {
        if (sPanelWindow != NULL)
            DestroyPanelWindow();
        return;
    }

    CreatePanelWindow();
    if (sPanelWindow == NULL || sPanelPixels == NULL)
        return;

    EnsureTables();
    if (sMatches == NULL)
        return;

    if (scriptedKey != 0)
        HandlePanelKey(scriptedKey);

    if (sStatusCountdown > 0)
    {
        if (--sStatusCountdown == 0)
        {
            sStatus[0] = '\0';
            sPanelDirty = true;
        }
    }

    if (sWarpRequested)
        ApplyQueuedWarp();

    ReportMapChange();

    if (sPanelDirty)
        RenderPanel();
}

// Closes the panel window and drops the match table. Only the panel's own
// window and renderer are destroyed here; SDL_Quit belongs to Platform_Cleanup.
void Platform_DevPanelShutdown(void)
{
    DestroyPanelWindow();

    if (sMatches != NULL)
    {
        free(sMatches);
        sMatches = NULL;
    }
}

// Called by the platform layer before AgbMain starts. The destination table is
// built here, while the engine is stopped, because it walks gMapGroups and the
// map layouts the engine rewrites during play.
void Platform_DevPanelInit(void)
{
    if (gPlatformDevPanelEnabled)
        EnsureTables();
}

// Builds the match table and the destination table on first use, so F3 can
// enable the panel on a run that never passed --dev-panel.
static void EnsureTables(void)
{
    if (sMatches != NULL)
        return;

    sMatches = malloc(sizeof(int) * DEV_PANEL_MAX_ROWS);
    if (sMatches == NULL)
    {
        fprintf(stderr, "[DevPanel] out of memory for the match table\n");
        gPlatformDevPanelEnabled = false;
        return;
    }

    BuildDestinations();
    RebuildMatches();
    sStatus[0] = '\0';
    sStatusCountdown = 0;
}

// Writes the panel's own pixels to a BMP, for headless verification of the
// panel window. Development aid only.
//
// The BMP is written here rather than through SDL_SaveBMP because saving a
// wrapping ARGB8888 surface made SDL quantise the image (the output held nine
// small index-like values instead of the panel's colours). A 24-bit bottom-up
// BMP is a few lines and has no conversion step to get wrong.
void Platform_DevPanelSaveScreenshot(const char *filename)
{
    FILE *file;
    u8 header[54];
    u32 imageSize;
    u32 fileSize;
    int y;
    bool ok = true;

    if (sPanelPixels == NULL)
    {
        fprintf(stderr, "[DevPanel] no panel pixels to save\n");
        return;
    }

    // 400 * 3 is already a multiple of 4, but compute the padded stride anyway.
    imageSize = (u32)DEV_PANEL_WIDTH * 3;
    imageSize = ((imageSize + 3) / 4) * 4 * DEV_PANEL_HEIGHT;
    fileSize = 54 + imageSize;

    memset(header, 0, sizeof(header));
    header[0] = 'B';
    header[1] = 'M';
    header[2] = (u8)(fileSize);
    header[3] = (u8)(fileSize >> 8);
    header[4] = (u8)(fileSize >> 16);
    header[5] = (u8)(fileSize >> 24);
    header[10] = 54; // pixel data offset
    header[14] = 40; // BITMAPINFOHEADER
    header[18] = (u8)(DEV_PANEL_WIDTH);
    header[19] = (u8)(DEV_PANEL_WIDTH >> 8);
    header[20] = (u8)(DEV_PANEL_WIDTH >> 16);
    header[21] = (u8)(DEV_PANEL_WIDTH >> 24);
    header[22] = (u8)(DEV_PANEL_HEIGHT);
    header[23] = (u8)(DEV_PANEL_HEIGHT >> 8);
    header[24] = (u8)(DEV_PANEL_HEIGHT >> 16);
    header[25] = (u8)(DEV_PANEL_HEIGHT >> 24);
    header[26] = 1; // planes
    header[28] = 24; // bits per pixel
    header[34] = (u8)(imageSize);
    header[35] = (u8)(imageSize >> 8);
    header[36] = (u8)(imageSize >> 16);
    header[37] = (u8)(imageSize >> 24);

    file = fopen(filename, "wb");
    if (file == NULL)
    {
        fprintf(stderr, "[DevPanel] cannot open %s\n", filename);
        return;
    }

    fwrite(header, 1, sizeof(header), file);

    // BMP rows run bottom-up and store BGR.
    for (y = DEV_PANEL_HEIGHT - 1; y >= 0 && ok; y--)
    {
        int x;

        for (x = 0; x < DEV_PANEL_WIDTH; x++)
        {
            u32 pixel = sPanelPixels[y * DEV_PANEL_WIDTH + x];
            u8 bgr[3];

            bgr[0] = (u8)(pixel & 0xFF);         // blue
            bgr[1] = (u8)((pixel >> 8) & 0xFF);  // green
            bgr[2] = (u8)((pixel >> 16) & 0xFF); // red

            if (fwrite(bgr, 1, 3, file) != 3)
            {
                ok = false;
                break;
            }
        }
    }

    fclose(file);

    if (ok)
        printf("[DevPanel] Saved panel screenshot to %s\n", filename);
    else
        fprintf(stderr, "[DevPanel] failed writing %s\n", filename);
}

// F3 toggling lives with the platform layer's key dispatch (src/platform/sdl2.c)
// because it must work whichever window has focus. Returns true when the key was
// the toggle.
bool Platform_DevPanelToggleKey(int key)
{
    if (key != SDLK_F3)
        return false;

    gPlatformDevPanelEnabled = !gPlatformDevPanelEnabled;
    if (gPlatformDevPanelEnabled)
    {
        CreatePanelWindow();
        sPanelDirty = true;
        printf("[DevPanel] enabled\n");
    }
    else
    {
        DestroyPanelWindow();
        printf("[DevPanel] disabled\n");
    }
    return true;
}
