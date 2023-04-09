/*****************************************************************************
 * Copyright (c) 2014-2023 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "../Identifiers.h"
#include "../common.h"
#include "../interface/Viewport.h"
#include "../object/Object.h"

class FootpathObject;
class FootpathSurfaceObject;
class FootpathRailingsObject;

enum
{
    PROVISIONAL_PATH_FLAG_SHOW_ARROW = (1 << 0),
    PROVISIONAL_PATH_FLAG_1 = (1 << 1),
    PROVISIONAL_PATH_FLAG_2 = (1 << 2),
};

constexpr auto FootpathMaxHeight = 248 * COORDS_Z_STEP;
constexpr auto FootpathMinHeight = 2 * COORDS_Z_STEP;
constexpr auto PATH_HEIGHT_STEP = 2 * COORDS_Z_STEP;
constexpr auto PATH_CLEARANCE = 4 * COORDS_Z_STEP;

enum class RailingEntrySupportType : uint8_t
{
    Box = 0,
    Pole = 1,
    Count
};

struct PathSurfaceDescriptor
{
    StringId Name;
    uint32_t Image;
    uint32_t PreviewImage;
    uint8_t Flags;
};

struct PathRailingsDescriptor
{
    StringId Name;
    uint32_t PreviewImage;
    uint32_t BridgeImage;
    uint32_t RailingsImage;
    RailingEntrySupportType SupportType;
    colour_t SupportColour;
    uint8_t Flags;
    uint8_t ScrollingMode;
};

using PathConstructFlags = uint8_t;
namespace PathConstructFlag
{
    constexpr PathConstructFlags IsQueue = 1 << 0;
    constexpr PathConstructFlags IsLegacyPathObject = 1 << 1;
} // namespace PathConstructFlag

struct FootpathSelection
{
    ObjectEntryIndex LegacyPath = OBJECT_ENTRY_INDEX_NULL;
    ObjectEntryIndex NormalSurface = OBJECT_ENTRY_INDEX_NULL;
    ObjectEntryIndex QueueSurface = OBJECT_ENTRY_INDEX_NULL;
    ObjectEntryIndex Railings = OBJECT_ENTRY_INDEX_NULL;
    bool IsQueueSelected{};

    ObjectEntryIndex GetSelectedSurface() const
    {
        return IsQueueSelected ? QueueSurface : NormalSurface;
    }
};

struct ProvisionalFootpath
{
    ObjectEntryIndex Type;
    CoordsXYZ Position;
    uint8_t Slope;
    uint8_t Flags;
    ObjectEntryIndex SurfaceIndex;
    ObjectEntryIndex RailingsIndex;
    PathConstructFlags ConstructFlags;
};

// Masks for values stored in TileElement.type
enum
{
    FOOTPATH_ELEMENT_TYPE_FLAG_IS_QUEUE = (1 << 0),
    FOOTPATH_ELEMENT_TYPE_FLAG_IS_WIDE = (1 << 1),
    FOOTPATH_ELEMENT_TYPE_DIRECTION_MASK = (1 << 6) | (1 << 7),
};

// Masks and flags for values stored in TileElement.properties.path.type
enum
{
    FOOTPATH_PROPERTIES_SLOPE_DIRECTION_MASK = (1 << 0) | (1 << 1),
    FOOTPATH_PROPERTIES_FLAG_IS_SLOPED = (1 << 2),
    FOOTPATH_PROPERTIES_FLAG_HAS_QUEUE_BANNER = (1 << 3),
    FOOTPATH_PROPERTIES_TYPE_MASK = (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7),
};

// Masks and flags for values stored in TileElement.properties.path.edges
enum
{
    FOOTPATH_PROPERTIES_EDGES_EDGES_MASK = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3),
    FOOTPATH_PROPERTIES_EDGES_CORNERS_MASK = (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7),
};

enum
{
    FOOTPATH_ELEMENT_FLAGS2_IS_SLOPED = 1 << 0,
    FOOTPATH_ELEMENT_FLAGS2_HAS_QUEUE_BANNER = (1 << 1),
    FOOTPATH_ELEMENT_FLAGS2_ADDITION_IS_GHOST = (1 << 2),
    FOOTPATH_ELEMENT_FLAGS2_BLOCKED_BY_VEHICLE = (1 << 3),
    FOOTPATH_ELEMENT_FLAGS2_ADDITION_IS_BROKEN = (1 << 4),
    FOOTPATH_ELEMENT_FLAGS2_LEGACY_PATH_ENTRY = (1 << 5),
    FOOTPATH_ELEMENT_FLAGS2_HAS_JUNCTION_RAILINGS = (1 << 6),
};

enum
{
    RAILING_ENTRY_FLAG_HAS_SUPPORT_BASE_SPRITE = (1 << 0),
    RAILING_ENTRY_FLAG_DRAW_PATH_OVER_SUPPORTS = (1 << 1), // When elevated
    RAILING_ENTRY_FLAG_NO_QUEUE_BANNER = (1 << 2),
};

enum
{
    FOOTPATH_SEARCH_SUCCESS,
    FOOTPATH_SEARCH_NOT_FOUND,
    FOOTPATH_SEARCH_INCOMPLETE,
    FOOTPATH_SEARCH_TOO_COMPLEX
};

enum
{
    SLOPE_IS_IRREGULAR_FLAG = (1 << 3), // Flag set in `DefaultPathSlope[]` and checked in `footpath_place_real`
    RAISE_FOOTPATH_FLAG = (1 << 4)
};

enum
{
    FOOTPATH_CORNER_0 = (1 << 0),
    FOOTPATH_CORNER_1 = (1 << 1),
    FOOTPATH_CORNER_2 = (1 << 2),
    FOOTPATH_CORNER_3 = (1 << 3),
};

enum
{
    FOOTPATH_CONNECTION_S = (1 << 0),
    FOOTPATH_CONNECTION_NE = (1 << 1),
    FOOTPATH_CONNECTION_W = (1 << 2),
    FOOTPATH_CONNECTION_SE = (1 << 3),
    FOOTPATH_CONNECTION_N = (1 << 4),
    FOOTPATH_CONNECTION_SW = (1 << 5),
    FOOTPATH_CONNECTION_E = (1 << 6),
    FOOTPATH_CONNECTION_NW = (1 << 7),
};

enum
{
    FOOTPATH_CONNECTED_MAP_EDGE_IGNORE_QUEUES = (1 << 0),
    FOOTPATH_CONNECTED_MAP_EDGE_UNOWN = (1 << 5),
    FOOTPATH_CONNECTED_MAP_EDGE_IGNORE_NO_ENTRY = (1 << 7)
};

extern FootpathSelection gFootpathSelection;
extern ProvisionalFootpath gProvisionalFootpath;
extern uint16_t gFootpathSelectedId;
extern CoordsXYZ gFootpathConstructFromPosition;
extern uint8_t gFootpathConstructSlope;
extern uint8_t gFootpathGroundFlags;

// Given a direction, this will return how to increase/decrease the x and y coordinates.
extern const std::array<CoordsXY, NumOrthogonalDirections> DirectionOffsets;
extern const std::array<CoordsXY, NumOrthogonalDirections> BinUseOffsets;
extern const std::array<CoordsXY, NumOrthogonalDirections * 2> BenchUseOffsets;

TileElement* MapGetFootpathElement(const CoordsXYZ& coords);
void FootpathInterruptPeeps(const CoordsXYZ& footpathPos);
money64 FootpathProvisionalSet(
    ObjectEntryIndex type, ObjectEntryIndex railingsType, const CoordsXYZ& footpathLoc, int32_t slope,
    PathConstructFlags constructFlags);
void FootpathProvisionalRemove();
void FootpathProvisionalUpdate();
CoordsXY FootpathGetCoordinatesFromPos(const ScreenCoordsXY& screenCoords, int32_t* direction, TileElement** tileElement);
CoordsXY FootpathBridgeGetInfoFromPos(const ScreenCoordsXY& screenCoords, int32_t* direction, TileElement** tileElement);
void FootpathRemoveLitter(const CoordsXYZ& footpathPos);
void FootpathConnectEdges(const CoordsXY& footpathPos, TileElement* tileElement, int32_t flags);
void FootpathUpdateQueueChains();
bool WallInTheWay(const CoordsXYRangedZ& fencePos, int32_t direction);
void FootpathChainRideQueue(
    RideId rideIndex, StationIndex entranceIndex, const CoordsXY& footpathPos, TileElement* tileElement, int32_t direction);
void FootpathUpdatePathWideFlags(const CoordsXY& footpathPos);
bool FootpathIsBlockedByVehicle(const TileCoordsXYZ& position);

int32_t FootpathIsConnectedToMapEdge(const CoordsXYZ& footpathPos, int32_t direction, int32_t flags);
void FootpathRemoveEdgesAt(const CoordsXY& footpathPos, TileElement* tileElement);

bool FootpathSelectDefault();
const FootpathObject* GetLegacyFootpathEntry(ObjectEntryIndex entryIndex);
const FootpathSurfaceObject* GetPathSurfaceEntry(ObjectEntryIndex entryIndex);
const FootpathRailingsObject* GetPathRailingsEntry(ObjectEntryIndex entryIndex);

void FootpathQueueChainReset();
void FootpathQueueChainPush(RideId rideIndex);
