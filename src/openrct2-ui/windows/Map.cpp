/*****************************************************************************
 * Copyright (c) 2014-2023 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <algorithm>
#include <openrct2-ui/interface/LandTool.h>
#include <openrct2-ui/interface/Viewport.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/windows/Window.h>
#include <openrct2/Cheats.h>
#include <openrct2/Context.h>
#include <openrct2/Game.h>
#include <openrct2/Input.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/actions/LandSetRightsAction.h>
#include <openrct2/actions/MapChangeSizeAction.h>
#include <openrct2/actions/ParkEntrancePlaceAction.h>
#include <openrct2/actions/PeepSpawnPlaceAction.h>
#include <openrct2/actions/SurfaceSetStyleAction.h>
#include <openrct2/audio/audio.h>
#include <openrct2/entity/EntityList.h>
#include <openrct2/entity/EntityRegistry.h>
#include <openrct2/entity/Staff.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/object/TerrainSurfaceObject.h>
#include <openrct2/ride/RideData.h>
#include <openrct2/ride/Track.h>
#include <openrct2/ride/TrainManager.h>
#include <openrct2/ride/Vehicle.h>
#include <openrct2/world/Entrance.h>
#include <openrct2/world/Footpath.h>
#include <openrct2/world/Scenery.h>
#include <openrct2/world/Surface.h>
#include <vector>

static constexpr uint16_t MapColour2(uint8_t colourA, uint8_t colourB)
{
    return (colourA << 8) | colourB;
}
static constexpr uint16_t MapColour(uint8_t colour)
{
    return MapColour2(colour, colour);
}
static constexpr uint16_t MapColourUnowned(uint16_t colour)
{
    return MapColour2((colour & 0xFF00) >> 8, PALETTE_INDEX_10);
}

constexpr int32_t MAP_WINDOW_MAP_SIZE = MAXIMUM_MAP_SIZE_TECHNICAL * 2;

static constexpr const StringId WINDOW_TITLE = STR_MAP_LABEL;
static constexpr const int32_t WH = 259;
static constexpr const int32_t WW = 245;

// Some functions manipulate coordinates on the map. These are the coordinates of the pixels in the
// minimap. In order to distinguish those from actual coordinates, we use a separate name.
using MapCoordsXY = TileCoordsXY;

enum
{
    PAGE_PEEPS,
    PAGE_RIDES
};

enum WindowMapWidgetIdx
{
    WIDX_BACKGROUND,
    WIDX_TITLE,
    WIDX_CLOSE,
    WIDX_RESIZE,
    WIDX_PEOPLE_TAB,
    WIDX_RIDES_TAB,
    WIDX_MAP,
    WIDX_MAP_SIZE_SPINNER_Y,
    WIDX_MAP_SIZE_SPINNER_Y_UP,
    WIDX_MAP_SIZE_SPINNER_Y_DOWN,
    WIDX_MAP_SIZE_LINK,
    WIDX_MAP_SIZE_SPINNER_X,
    WIDX_MAP_SIZE_SPINNER_X_UP,
    WIDX_MAP_SIZE_SPINNER_X_DOWN,
    WIDX_SET_LAND_RIGHTS,
    WIDX_BUILD_PARK_ENTRANCE,
    WIDX_PEOPLE_STARTING_POSITION,
    WIDX_LAND_TOOL,
    WIDX_LAND_TOOL_SMALLER,
    WIDX_LAND_TOOL_LARGER,
    WIDX_LAND_OWNED_CHECKBOX,
    WIDX_CONSTRUCTION_RIGHTS_OWNED_CHECKBOX,
    WIDX_LAND_SALE_CHECKBOX,
    WIDX_CONSTRUCTION_RIGHTS_SALE_CHECKBOX,
    WIDX_ROTATE_90,
    WIDX_MAP_GENERATOR
};

validate_global_widx(WC_MAP, WIDX_ROTATE_90);

// clang-format off
static Widget window_map_widgets[] = {
    WINDOW_SHIM(WINDOW_TITLE, WW, WH),
    MakeWidget        ({  0,  43}, {245, 215}, WindowWidgetType::Resize,    WindowColour::Secondary                                                                                  ),
    MakeRemapWidget   ({  3,  17}, { 31,  27}, WindowWidgetType::ColourBtn, WindowColour::Secondary, SPR_TAB,                         STR_SHOW_PEOPLE_ON_MAP_TIP                     ),
    MakeRemapWidget   ({ 34,  17}, { 31,  27}, WindowWidgetType::ColourBtn, WindowColour::Secondary, SPR_TAB,                         STR_SHOW_RIDES_STALLS_ON_MAP_TIP               ),
    MakeWidget        ({  3,  46}, {239, 180}, WindowWidgetType::Scroll,    WindowColour::Secondary, SCROLL_BOTH                                                                     ),
    MakeSpinnerWidgets({102, 229}, { 50,  12}, WindowWidgetType::Spinner,   WindowColour::Secondary, STR_COMMA16                                                                     ), // NB: 3 widgets
    MakeWidget        ({153, 230}, { 20,  12}, WindowWidgetType::FlatBtn,   WindowColour::Secondary, ImageId(SPR_G2_LINK_CHAIN),               STR_MAINTAIN_SQUARE_MAP_TOOLTIP                ),
    MakeSpinnerWidgets({174, 229}, { 50,  12}, WindowWidgetType::Spinner,   WindowColour::Secondary, STR_POP16_COMMA16                                                               ), // NB: 3 widgets
    MakeWidget        ({  4,   1}, { 24,  24}, WindowWidgetType::FlatBtn,   WindowColour::Secondary, ImageId(SPR_BUY_LAND_RIGHTS),             STR_SELECT_PARK_OWNED_LAND_TIP                 ),
    MakeWidget        ({  4,   1}, { 24,  24}, WindowWidgetType::FlatBtn,   WindowColour::Secondary, ImageId(SPR_PARK_ENTRANCE),               STR_BUILD_PARK_ENTRANCE_TIP                    ),
    MakeWidget        ({ 28,   1}, { 24,  24}, WindowWidgetType::FlatBtn,   WindowColour::Secondary, 0xFFFFFFFF,                      STR_SET_STARTING_POSITIONS_TIP                 ),
    MakeWidget        ({  4,  17}, { 44,  32}, WindowWidgetType::ImgBtn,    WindowColour::Secondary, ImageId(SPR_LAND_TOOL_SIZE_0)                                                            ),
    MakeRemapWidget   ({  5,  18}, { 16,  16}, WindowWidgetType::TrnBtn,    WindowColour::Secondary, SPR_LAND_TOOL_DECREASE,          STR_ADJUST_SMALLER_LAND_TIP                    ),
    MakeRemapWidget   ({ 31,  32}, { 16,  16}, WindowWidgetType::TrnBtn,    WindowColour::Secondary, SPR_LAND_TOOL_INCREASE,          STR_ADJUST_LARGER_LAND_TIP                     ),
    MakeWidget        ({ 58, 197}, {184,  12}, WindowWidgetType::Checkbox,  WindowColour::Secondary, STR_LAND_OWNED,                  STR_SET_LAND_TO_BE_OWNED_TIP                   ),
    MakeWidget        ({ 58, 197}, {184,  12}, WindowWidgetType::Checkbox,  WindowColour::Secondary, STR_CONSTRUCTION_RIGHTS_OWNED,   STR_SET_CONSTRUCTION_RIGHTS_TO_BE_OWNED_TIP    ),
    MakeWidget        ({ 58, 197}, {184,  12}, WindowWidgetType::Checkbox,  WindowColour::Secondary, STR_LAND_SALE,                   STR_SET_LAND_TO_BE_AVAILABLE_TIP               ),
    MakeWidget        ({ 58, 197}, {174,  12}, WindowWidgetType::Checkbox,  WindowColour::Secondary, STR_CONSTRUCTION_RIGHTS_SALE,    STR_SET_CONSTRUCTION_RIGHTS_TO_BE_AVAILABLE_TIP),
    MakeWidget        ({218,  45}, { 24,  24}, WindowWidgetType::FlatBtn,   WindowColour::Secondary, ImageId(SPR_ROTATE_ARROW),                STR_ROTATE_OBJECTS_90                          ),
    MakeWidget        ({110, 189}, {131,  14}, WindowWidgetType::Button,    WindowColour::Secondary, STR_MAPGEN_WINDOW_TITLE,         STR_MAP_GENERATOR_TIP                          ),
    WIDGETS_END,
};

// used in transforming viewport view coordinates to minimap coordinates
// rct2: 0x00981BBC
static constexpr const ScreenCoordsXY MiniMapOffsets[] = {
    {     MAXIMUM_MAP_SIZE_TECHNICAL - 8,                              0 },
    { 2 * MAXIMUM_MAP_SIZE_TECHNICAL - 8,     MAXIMUM_MAP_SIZE_TECHNICAL },
    {     MAXIMUM_MAP_SIZE_TECHNICAL - 8, 2 * MAXIMUM_MAP_SIZE_TECHNICAL },
    {                              0 - 8,     MAXIMUM_MAP_SIZE_TECHNICAL },
};
// clang-format on

class MapWindow final : public Window
{
    uint8_t _rotation;

public:
    MapWindow()
    {
        _mapImageData.resize(MAP_WINDOW_MAP_SIZE * MAP_WINDOW_MAP_SIZE);
    }

    void OnOpen() override
    {
        widgets = window_map_widgets;

        hold_down_widgets = (1uLL << WIDX_MAP_SIZE_SPINNER_Y_UP) | (1uLL << WIDX_MAP_SIZE_SPINNER_Y_DOWN)
            | (1uLL << WIDX_MAP_SIZE_SPINNER_X_UP) | (1uLL << WIDX_MAP_SIZE_SPINNER_X_DOWN) | (1uLL << WIDX_LAND_TOOL_LARGER)
            | (1uLL << WIDX_LAND_TOOL_SMALLER);

        InitScrollWidgets();

        _rotation = GetCurrentRotation();

        InitMap();
        gWindowSceneryRotation = 0;
        CentreMapOnViewPoint();
        FootpathSelectDefault();

        _mapWidthAndHeightLinked = gMapSize.x == gMapSize.y;

        // Reset land rights tool size
        _landRightsToolSize = 1;
    }

    void OnClose() override
    {
        _mapImageData.clear();
        _mapImageData.shrink_to_fit();
        if ((InputTestFlag(INPUT_FLAG_TOOL_ACTIVE)) && gCurrentToolWidget.window_classification == classification
            && gCurrentToolWidget.window_number == number)
        {
            ToolCancel();
        }
    }

    void OnResize() override
    {
        flags |= WF_RESIZABLE;
        min_width = 245;
        max_width = 800;
        min_height = 259;
        max_height = 560;
    }

    void OnMouseUp(WidgetIndex widgetIndex) override
    {
        switch (widgetIndex)
        {
            case WIDX_CLOSE:
                Close();
                break;
            case WIDX_SET_LAND_RIGHTS:
                Invalidate();
                if (ToolSet(*this, widgetIndex, Tool::UpArrow))
                    break;
                _activeTool = 2;
                // Prevent mountain tool size.
                _landRightsToolSize = std::max<uint16_t>(MINIMUM_TOOL_SIZE, _landRightsToolSize);
                ShowGridlines();
                ShowLandRights();
                ShowConstructionRights();
                break;
            case WIDX_LAND_OWNED_CHECKBOX:
                _activeTool ^= 2;

                if (_activeTool & 2)
                    _activeTool &= 0xF2;

                Invalidate();
                break;
            case WIDX_LAND_SALE_CHECKBOX:
                _activeTool ^= 8;

                if (_activeTool & 8)
                    _activeTool &= 0xF8;

                Invalidate();
                break;
            case WIDX_CONSTRUCTION_RIGHTS_OWNED_CHECKBOX:
                _activeTool ^= 1;

                if (_activeTool & 1)
                    _activeTool &= 0xF1;

                Invalidate();
                break;
            case WIDX_CONSTRUCTION_RIGHTS_SALE_CHECKBOX:
                _activeTool ^= 4;

                if (_activeTool & 4)
                    _activeTool &= 0xF4;

                Invalidate();
                break;
            case WIDX_BUILD_PARK_ENTRANCE:
                Invalidate();
                if (ToolSet(*this, widgetIndex, Tool::UpArrow))
                    break;

                gParkEntranceGhostExists = false;
                InputSetFlag(INPUT_FLAG_6, true);

                ShowGridlines();
                ShowLandRights();
                ShowConstructionRights();
                break;
            case WIDX_ROTATE_90:
                gWindowSceneryRotation = (gWindowSceneryRotation + 1) & 3;
                break;
            case WIDX_PEOPLE_STARTING_POSITION:
                if (ToolSet(*this, widgetIndex, Tool::UpArrow))
                    break;

                ShowGridlines();
                ShowLandRights();
                ShowConstructionRights();
                break;
            case WIDX_LAND_TOOL:
                InputLandSize();
                break;
            case WIDX_MAP_SIZE_SPINNER_Y:
            case WIDX_MAP_SIZE_SPINNER_X:
                InputMapSize(widgetIndex);
                break;
            case WIDX_MAP_SIZE_LINK:
                _mapWidthAndHeightLinked = !_mapWidthAndHeightLinked;
                break;
            case WIDX_MAP_GENERATOR:
                ContextOpenWindow(WindowClass::Mapgen);
                break;
            default:
                if (widgetIndex >= WIDX_PEOPLE_TAB && widgetIndex <= WIDX_RIDES_TAB)
                {
                    widgetIndex -= WIDX_PEOPLE_TAB;
                    if (widgetIndex == selected_tab)
                        break;

                    selected_tab = widgetIndex;
                    list_information_type = 0;
                }
        }
    }

    void OnMouseDown(WidgetIndex widgetIndex) override
    {
        switch (widgetIndex)
        {
            case WIDX_MAP_SIZE_SPINNER_Y_UP:
                _resizeDirection = ResizeDirection::Y;
                IncreaseMapSize();
                break;
            case WIDX_MAP_SIZE_SPINNER_Y_DOWN:
                _resizeDirection = ResizeDirection::Y;
                DecreaseMapSize();
                break;
            case WIDX_MAP_SIZE_SPINNER_X_UP:
                _resizeDirection = ResizeDirection::X;
                IncreaseMapSize();
                break;
            case WIDX_MAP_SIZE_SPINNER_X_DOWN:
                _resizeDirection = ResizeDirection::X;
                DecreaseMapSize();
                break;
            case WIDX_LAND_TOOL_SMALLER:
                // Decrement land rights tool size
                _landRightsToolSize = std::max(MINIMUM_TOOL_SIZE, _landRightsToolSize - 1);

                Invalidate();
                break;
            case WIDX_LAND_TOOL_LARGER:
                // Increment land rights tool size
                _landRightsToolSize = std::min(MAXIMUM_TOOL_SIZE, _landRightsToolSize + 1);

                Invalidate();
                break;
        }
    }

    void OnUpdate() override
    {
        if (GetCurrentRotation() != _rotation)
        {
            _rotation = GetCurrentRotation();
            InitMap();
            CentreMapOnViewPoint();
        }

        for (int32_t i = 0; i < 16; i++)
            SetMapPixels();

        Invalidate();

        // Update tab animations
        list_information_type++;
        switch (selected_tab)
        {
            case PAGE_PEEPS:
                if (list_information_type >= 32)
                {
                    list_information_type = 0;
                }
                break;
            case PAGE_RIDES:
                if (list_information_type >= 64)
                {
                    list_information_type = 0;
                }
                break;
        }
    }

    void OnToolUpdate(WidgetIndex widgetIndex, const ScreenCoordsXY& screenCoords) override
    {
        switch (widgetIndex)
        {
            case WIDX_SET_LAND_RIGHTS:
                SetLandRightsToolUpdate(screenCoords);
                break;
            case WIDX_BUILD_PARK_ENTRANCE:
                PlaceParkEntranceToolUpdate(screenCoords);
                break;
            case WIDX_PEOPLE_STARTING_POSITION:
                SetPeepSpawnToolUpdate(screenCoords);
                break;
        }
    }

    void OnToolDown(WidgetIndex widgetIndex, const ScreenCoordsXY& screenCoords) override
    {
        switch (widgetIndex)
        {
            case WIDX_BUILD_PARK_ENTRANCE:
                PlaceParkEntranceToolDown(screenCoords);
                break;
            case WIDX_PEOPLE_STARTING_POSITION:
                SetPeepSpawnToolDown(screenCoords);
                break;
        }
    }

    void OnToolDrag(WidgetIndex widgetIndex, const ScreenCoordsXY& screenCoords) override
    {
        switch (widgetIndex)
        {
            case WIDX_SET_LAND_RIGHTS:
                if (gMapSelectFlags & MAP_SELECT_FLAG_ENABLE)
                {
                    auto landSetRightsAction = LandSetRightsAction(
                        { gMapSelectPositionA.x, gMapSelectPositionA.y, gMapSelectPositionB.x, gMapSelectPositionB.y },
                        LandSetRightSetting::SetOwnershipWithChecks, _activeTool << 4);
                    GameActions::Execute(&landSetRightsAction);
                }
                break;
        }
    }

    void OnToolAbort(WidgetIndex widgetIndex) override
    {
        switch (widgetIndex)
        {
            case WIDX_SET_LAND_RIGHTS:
                Invalidate();
                HideGridlines();
                HideLandRights();
                HideConstructionRights();
                break;
            case WIDX_BUILD_PARK_ENTRANCE:
                ParkEntranceRemoveGhost();
                Invalidate();
                HideGridlines();
                HideLandRights();
                HideConstructionRights();
                break;
            case WIDX_PEOPLE_STARTING_POSITION:
                Invalidate();
                HideGridlines();
                HideLandRights();
                HideConstructionRights();
                break;
        }
    }

    void SetLandRightsToolUpdate(const ScreenCoordsXY& screenCoords)
    {
        MapInvalidateSelectionRect();
        gMapSelectFlags &= ~MAP_SELECT_FLAG_ENABLE;
        auto mapCoords = ScreenGetMapXY(screenCoords, nullptr);
        if (!mapCoords.has_value())
            return;

        gMapSelectFlags |= MAP_SELECT_FLAG_ENABLE;
        gMapSelectType = MAP_SELECT_TYPE_FULL;

        int32_t landRightsToolSize = _landRightsToolSize;
        if (landRightsToolSize == 0)
            landRightsToolSize = 1;

        int32_t size = (landRightsToolSize * 32) - 32;
        int32_t radius = (landRightsToolSize * 16) - 16;
        mapCoords->x = (mapCoords->x - radius) & 0xFFE0;
        mapCoords->y = (mapCoords->y - radius) & 0xFFE0;
        gMapSelectPositionA = *mapCoords;
        gMapSelectPositionB.x = mapCoords->x + size;
        gMapSelectPositionB.y = mapCoords->y + size;
        MapInvalidateSelectionRect();
    }

    CoordsXYZD PlaceParkEntranceGetMapPosition(const ScreenCoordsXY& screenCoords)
    {
        CoordsXYZD parkEntranceMapPosition{ 0, 0, 0, INVALID_DIRECTION };
        const CoordsXY mapCoords = ViewportInteractionGetTileStartAtCursor(screenCoords);
        parkEntranceMapPosition = { mapCoords.x, mapCoords.y, 0, INVALID_DIRECTION };
        if (parkEntranceMapPosition.IsNull())
            return parkEntranceMapPosition;

        auto surfaceElement = MapGetSurfaceElementAt(mapCoords);
        if (surfaceElement == nullptr)
        {
            parkEntranceMapPosition.SetNull();
            return parkEntranceMapPosition;
        }

        parkEntranceMapPosition.z = surfaceElement->GetWaterHeight();
        if (parkEntranceMapPosition.z == 0)
        {
            parkEntranceMapPosition.z = surfaceElement->GetBaseZ();
            if ((surfaceElement->GetSlope() & TILE_ELEMENT_SLOPE_ALL_CORNERS_UP) != 0)
            {
                parkEntranceMapPosition.z += 16;
                if (surfaceElement->GetSlope() & TILE_ELEMENT_SLOPE_DOUBLE_HEIGHT)
                {
                    parkEntranceMapPosition.z += 16;
                }
            }
        }
        parkEntranceMapPosition.direction = (gWindowSceneryRotation - GetCurrentRotation()) & 3;
        return parkEntranceMapPosition;
    }

    void PlaceParkEntranceToolUpdate(const ScreenCoordsXY& screenCoords)
    {
        MapInvalidateSelectionRect();
        MapInvalidateMapSelectionTiles();
        gMapSelectFlags &= ~MAP_SELECT_FLAG_ENABLE;
        gMapSelectFlags &= ~MAP_SELECT_FLAG_ENABLE_ARROW;
        gMapSelectFlags &= ~MAP_SELECT_FLAG_ENABLE_CONSTRUCT;
        CoordsXYZD parkEntrancePosition = PlaceParkEntranceGetMapPosition(screenCoords);
        if (parkEntrancePosition.IsNull())
        {
            ParkEntranceRemoveGhost();
            return;
        }

        int32_t sideDirection = (parkEntrancePosition.direction + 1) & 3;
        gMapSelectionTiles.clear();
        gMapSelectionTiles.push_back({ parkEntrancePosition.x, parkEntrancePosition.y });
        gMapSelectionTiles.push_back({ parkEntrancePosition.x + CoordsDirectionDelta[sideDirection].x,
                                       parkEntrancePosition.y + CoordsDirectionDelta[sideDirection].y });
        gMapSelectionTiles.push_back({ parkEntrancePosition.x - CoordsDirectionDelta[sideDirection].x,
                                       parkEntrancePosition.y - CoordsDirectionDelta[sideDirection].y });

        gMapSelectArrowPosition = parkEntrancePosition;
        gMapSelectArrowDirection = parkEntrancePosition.direction;

        gMapSelectFlags |= MAP_SELECT_FLAG_ENABLE_CONSTRUCT | MAP_SELECT_FLAG_ENABLE_ARROW;
        MapInvalidateMapSelectionTiles();
        if (gParkEntranceGhostExists && parkEntrancePosition == gParkEntranceGhostPosition)
        {
            return;
        }

        ParkEntranceRemoveGhost();

        auto gameAction = ParkEntrancePlaceAction(parkEntrancePosition, gFootpathSelectedId);
        gameAction.SetFlags(GAME_COMMAND_FLAG_GHOST);

        auto result = GameActions::Execute(&gameAction);
        if (result.Error == GameActions::Status::Ok)
        {
            gParkEntranceGhostPosition = parkEntrancePosition;
            gParkEntranceGhostExists = true;
        }
    }

    void PlaceParkEntranceToolDown(const ScreenCoordsXY& screenCoords)
    {
        ParkEntranceRemoveGhost();

        CoordsXYZD parkEntrancePosition = PlaceParkEntranceGetMapPosition(screenCoords);
        if (!parkEntrancePosition.IsNull())
        {
            auto gameAction = ParkEntrancePlaceAction(parkEntrancePosition, gFootpathSelectedId);
            auto result = GameActions::Execute(&gameAction);
            if (result.Error == GameActions::Status::Ok)
            {
                OpenRCT2::Audio::Play3D(OpenRCT2::Audio::SoundId::PlaceItem, result.Position);
            }
        }
    }

    void SetPeepSpawnToolUpdate(const ScreenCoordsXY& screenCoords)
    {
        int32_t direction;
        TileElement* tileElement;
        MapInvalidateSelectionRect();
        gMapSelectFlags &= ~MAP_SELECT_FLAG_ENABLE;
        gMapSelectFlags &= ~MAP_SELECT_FLAG_ENABLE_ARROW;
        auto mapCoords = FootpathBridgeGetInfoFromPos(screenCoords, &direction, &tileElement);
        if (mapCoords.IsNull())
            return;

        int32_t mapZ = tileElement->GetBaseZ();
        if (tileElement->GetType() == TileElementType::Surface)
        {
            if ((tileElement->AsSurface()->GetSlope() & TILE_ELEMENT_SLOPE_ALL_CORNERS_UP) != 0)
                mapZ += 16;
            if (tileElement->AsSurface()->GetSlope() & TILE_ELEMENT_SLOPE_DOUBLE_HEIGHT)
                mapZ += 16;
        }

        gMapSelectFlags |= MAP_SELECT_FLAG_ENABLE;
        gMapSelectFlags |= MAP_SELECT_FLAG_ENABLE_ARROW;
        gMapSelectType = MAP_SELECT_TYPE_FULL;
        gMapSelectPositionA = mapCoords;
        gMapSelectPositionB = mapCoords;
        gMapSelectArrowPosition = CoordsXYZ{ mapCoords, mapZ };
        gMapSelectArrowDirection = DirectionReverse(direction);
        MapInvalidateSelectionRect();
    }

    void SetPeepSpawnToolDown(const ScreenCoordsXY& screenCoords)
    {
        // Verify footpath exists at location, and retrieve coordinates
        TileElement* tileElement;
        int32_t direction;
        auto mapCoords = FootpathGetCoordinatesFromPos(screenCoords, &direction, &tileElement);
        if (mapCoords.IsNull())
            return;

        int32_t mapZ = tileElement->GetBaseZ();

        auto gameAction = PeepSpawnPlaceAction({ mapCoords, mapZ, static_cast<Direction>(direction) });
        auto result = GameActions::Execute(&gameAction);
        if (result.Error == GameActions::Status::Ok)
        {
            OpenRCT2::Audio::Play3D(OpenRCT2::Audio::SoundId::PlaceItem, result.Position);
        }
    }

    void OnTextInput(WidgetIndex widgetIndex, std::string_view text) override
    {
        if (text.empty())
            return;

        switch (widgetIndex)
        {
            case WIDX_LAND_TOOL:
            {
                char* end;
                std::string textStr = std::string(text);
                int32_t size = strtol(textStr.c_str(), &end, 10);
                if (*end == '\0')
                {
                    size = std::clamp(size, MINIMUM_TOOL_SIZE, MAXIMUM_TOOL_SIZE);
                    _landRightsToolSize = size;
                    Invalidate();
                }
                break;
            }
            case WIDX_MAP_SIZE_SPINNER_Y:
            case WIDX_MAP_SIZE_SPINNER_X:
            {
                char* end;
                std::string textStr = std::string(text);
                int32_t size = strtol(textStr.c_str(), &end, 10);
                if (*end == '\0')
                {
                    // The practical size is 2 lower than the technical size
                    size += 2;
                    size = std::clamp(size, MINIMUM_MAP_SIZE_TECHNICAL, MAXIMUM_MAP_SIZE_TECHNICAL);

                    TileCoordsXY newMapSize = gMapSize;
                    if (_resizeDirection != ResizeDirection::X)
                        newMapSize.y = size;
                    if (_resizeDirection != ResizeDirection::Y)
                        newMapSize.x = size;

                    auto mapChangeSizeAction = MapChangeSizeAction(newMapSize);
                    GameActions::Execute(&mapChangeSizeAction);
                    Invalidate();
                }
                break;
            }
        }
    }

    ScreenSize OnScrollGetSize(int32_t scrollIndex) override
    {
        return ScreenSize(MAP_WINDOW_MAP_SIZE, MAP_WINDOW_MAP_SIZE);
    }

    void OnScrollMouseDown(int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
    {
        CoordsXY c = ScreenToMap(screenCoords);
        auto mapCoords = CoordsXY{ std::clamp(c.x, 0, MAXIMUM_MAP_SIZE_BIG - 1), std::clamp(c.y, 0, MAXIMUM_MAP_SIZE_BIG - 1) };
        auto mapZ = TileElementHeight(mapCoords);

        WindowBase* mainWindow = WindowGetMain();
        if (mainWindow != nullptr)
        {
            WindowScrollToLocation(*mainWindow, { mapCoords, mapZ });
        }

        if (LandToolIsActive())
        {
            // Set land terrain
            int32_t landToolSize = std::max<int32_t>(1, gLandToolSize);
            int32_t size = (landToolSize * 32) - 32;
            int32_t radius = (landToolSize * 16) - 16;

            mapCoords = (mapCoords - CoordsXY{ radius, radius }).ToTileStart();
            MapInvalidateSelectionRect();
            gMapSelectFlags |= MAP_SELECT_FLAG_ENABLE;
            gMapSelectType = MAP_SELECT_TYPE_FULL;
            gMapSelectPositionA = mapCoords;
            gMapSelectPositionB = mapCoords + CoordsXY{ size, size };
            MapInvalidateSelectionRect();

            auto surfaceSetStyleAction = SurfaceSetStyleAction(
                { gMapSelectPositionA.x, gMapSelectPositionA.y, gMapSelectPositionB.x, gMapSelectPositionB.y },
                gLandToolTerrainSurface, gLandToolTerrainEdge);
            GameActions::Execute(&surfaceSetStyleAction);
        }
        else if (WidgetIsActiveTool(*this, WIDX_SET_LAND_RIGHTS))
        {
            // Set land rights
            int32_t landRightsToolSize = std::max<int32_t>(1, _landRightsToolSize);
            int32_t size = (landRightsToolSize * 32) - 32;
            int32_t radius = (landRightsToolSize * 16) - 16;
            mapCoords = (mapCoords - CoordsXY{ radius, radius }).ToTileStart();

            MapInvalidateSelectionRect();
            gMapSelectFlags |= MAP_SELECT_FLAG_ENABLE;
            gMapSelectType = MAP_SELECT_TYPE_FULL;
            gMapSelectPositionA = mapCoords;
            gMapSelectPositionB = mapCoords + CoordsXY{ size, size };
            MapInvalidateSelectionRect();

            auto landSetRightsAction = LandSetRightsAction(
                { gMapSelectPositionA.x, gMapSelectPositionA.y, gMapSelectPositionB.x, gMapSelectPositionB.y },
                LandSetRightSetting::SetOwnershipWithChecks, _activeTool << 4);
            GameActions::Execute(&landSetRightsAction);
        }
    }

    void OnScrollMouseDrag(int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
    {
        OnScrollMouseDown(scrollIndex, screenCoords);
    }

    void OnScrollDraw(int32_t scrollIndex, DrawPixelInfo& dpi) override
    {
        GfxClear(&dpi, PALETTE_INDEX_10);

        G1Element g1temp = {};
        g1temp.offset = _mapImageData.data();
        g1temp.width = MAP_WINDOW_MAP_SIZE;
        g1temp.height = MAP_WINDOW_MAP_SIZE;
        g1temp.x_offset = -8;
        g1temp.y_offset = -8;
        GfxSetG1Element(SPR_TEMP, &g1temp);
        DrawingEngineInvalidateImage(SPR_TEMP);
        GfxDrawSprite(&dpi, ImageId(SPR_TEMP), { 0, 0 });

        if (selected_tab == PAGE_PEEPS)
        {
            PaintPeepOverlay(&dpi);
        }
        else
        {
            PaintTrainOverlay(&dpi);
        }
        PaintHudRectangle(&dpi);
    }

    void OnPrepareDraw() override
    {
        // Set the pressed widgets
        pressed_widgets = 0;
        SetWidgetPressed(WIDX_MAP_SIZE_LINK, _mapWidthAndHeightLinked);
        pressed_widgets |= (1uLL << (WIDX_PEOPLE_TAB + selected_tab));
        pressed_widgets |= (1uLL << WIDX_LAND_TOOL);

        if (_activeTool & (1 << 3))
            pressed_widgets |= (1uLL << WIDX_LAND_SALE_CHECKBOX);

        if (_activeTool & (1 << 2))
            pressed_widgets |= (1uLL << WIDX_CONSTRUCTION_RIGHTS_SALE_CHECKBOX);

        if (_activeTool & (1 << 1))
            pressed_widgets |= (1uLL << WIDX_LAND_OWNED_CHECKBOX);

        if (_activeTool & (1 << 0))
            pressed_widgets |= (1uLL << WIDX_CONSTRUCTION_RIGHTS_OWNED_CHECKBOX);

        // Set disabled widgets
        SetWidgetDisabled(WIDX_MAP_SIZE_LINK, gMapSize.x != gMapSize.y);

        // Resize widgets to window size
        ResizeFrameWithPage();
        widgets[WIDX_MAP].right = width - 4;

        if ((gScreenFlags & SCREEN_FLAGS_SCENARIO_EDITOR) || gCheatsSandboxMode)
            widgets[WIDX_MAP].bottom = height - 1 - 72;
        else if (selected_tab == PAGE_RIDES)
            widgets[WIDX_MAP].bottom = height - 1 - (4 * LIST_ROW_HEIGHT + 4);
        else
            widgets[WIDX_MAP].bottom = height - 1 - 14;

        widgets[WIDX_MAP_SIZE_SPINNER_Y].top = height - 15;
        widgets[WIDX_MAP_SIZE_SPINNER_Y].bottom = height - 4;
        widgets[WIDX_MAP_SIZE_SPINNER_Y_UP].top = height - 14;
        widgets[WIDX_MAP_SIZE_SPINNER_Y_UP].bottom = height - 5;
        widgets[WIDX_MAP_SIZE_SPINNER_Y_DOWN].top = height - 14;
        widgets[WIDX_MAP_SIZE_SPINNER_Y_DOWN].bottom = height - 5;
        widgets[WIDX_MAP_SIZE_LINK].top = height - 15;
        widgets[WIDX_MAP_SIZE_LINK].bottom = height - 4;
        widgets[WIDX_MAP_SIZE_SPINNER_X].top = height - 15;
        widgets[WIDX_MAP_SIZE_SPINNER_X].bottom = height - 4;
        widgets[WIDX_MAP_SIZE_SPINNER_X_UP].top = height - 14;
        widgets[WIDX_MAP_SIZE_SPINNER_X_UP].bottom = height - 5;
        widgets[WIDX_MAP_SIZE_SPINNER_X_DOWN].top = height - 14;
        widgets[WIDX_MAP_SIZE_SPINNER_X_DOWN].bottom = height - 5;

        widgets[WIDX_SET_LAND_RIGHTS].top = height - 70;
        widgets[WIDX_SET_LAND_RIGHTS].bottom = height - 70 + 23;
        widgets[WIDX_BUILD_PARK_ENTRANCE].top = height - 46;
        widgets[WIDX_BUILD_PARK_ENTRANCE].bottom = height - 46 + 23;
        widgets[WIDX_ROTATE_90].top = height - 46;
        widgets[WIDX_ROTATE_90].bottom = height - 46 + 23;
        widgets[WIDX_PEOPLE_STARTING_POSITION].top = height - 46;
        widgets[WIDX_PEOPLE_STARTING_POSITION].bottom = height - 46 + 23;

        widgets[WIDX_LAND_TOOL].top = height - 42;
        widgets[WIDX_LAND_TOOL].bottom = height - 42 + 30;
        widgets[WIDX_LAND_TOOL_SMALLER].top = height - 41;
        widgets[WIDX_LAND_TOOL_SMALLER].bottom = height - 41 + 15;
        widgets[WIDX_LAND_TOOL_LARGER].top = height - 27;
        widgets[WIDX_LAND_TOOL_LARGER].bottom = height - 27 + 15;

        widgets[WIDX_MAP_GENERATOR].top = height - 69;
        widgets[WIDX_MAP_GENERATOR].bottom = height - 69 + 13;

        // Land tool mode (4 checkboxes)
        int checkboxY = height - 55;
        for (int32_t i = WIDX_LAND_OWNED_CHECKBOX; i <= WIDX_CONSTRUCTION_RIGHTS_SALE_CHECKBOX; i++)
        {
            widgets[i].top = checkboxY;
            checkboxY += 11;
            widgets[i].bottom = checkboxY;
            checkboxY += 2;
        }

        // Disable all scenario editor related widgets
        for (int32_t i = WIDX_MAP_SIZE_SPINNER_Y; i <= WIDX_MAP_GENERATOR; i++)
        {
            widgets[i].type = WindowWidgetType::Empty;
        }

        if ((gScreenFlags & SCREEN_FLAGS_SCENARIO_EDITOR) || gCheatsSandboxMode)
        {
            // scenario editor: build park entrance selected, show rotate button
            if ((InputTestFlag(INPUT_FLAG_TOOL_ACTIVE)) && gCurrentToolWidget.window_classification == WindowClass::Map
                && gCurrentToolWidget.widget_index == WIDX_BUILD_PARK_ENTRANCE)
            {
                widgets[WIDX_ROTATE_90].type = WindowWidgetType::FlatBtn;
            }

            // Always show set land rights button
            widgets[WIDX_SET_LAND_RIGHTS].type = WindowWidgetType::FlatBtn;

            // If any tool is active
            if ((InputTestFlag(INPUT_FLAG_TOOL_ACTIVE)) && gCurrentToolWidget.window_classification == WindowClass::Map)
            {
                // if not in set land rights mode: show the default scenario editor buttons
                if (gCurrentToolWidget.widget_index != WIDX_SET_LAND_RIGHTS)
                {
                    ShowDefaultScenarioEditorButtons();
                }
                else
                { // if in set land rights mode: show land tool buttons + modes
                    widgets[WIDX_LAND_TOOL].type = WindowWidgetType::ImgBtn;
                    widgets[WIDX_LAND_TOOL_SMALLER].type = WindowWidgetType::TrnBtn;
                    widgets[WIDX_LAND_TOOL_LARGER].type = WindowWidgetType::TrnBtn;

                    for (int32_t i = WIDX_LAND_OWNED_CHECKBOX; i <= WIDX_CONSTRUCTION_RIGHTS_SALE_CHECKBOX; i++)
                        widgets[i].type = WindowWidgetType::Checkbox;

                    widgets[WIDX_LAND_TOOL].image = ImageId(LandTool::SizeToSpriteIndex(_landRightsToolSize));
                }
            }
            else
            {
                // if no tool is active: show the default scenario editor buttons
                ShowDefaultScenarioEditorButtons();
            }
        }
    }

    void OnDraw(DrawPixelInfo& dpi) override
    {
        DrawWidgets(dpi);
        DrawTabImages(&dpi);

        auto screenCoords = windowPos
            + ScreenCoordsXY{ window_map_widgets[WIDX_LAND_TOOL].midX(), window_map_widgets[WIDX_LAND_TOOL].midY() };

        // Draw land tool size
        if (WidgetIsActiveTool(*this, WIDX_SET_LAND_RIGHTS) && _landRightsToolSize > MAX_TOOL_SIZE_WITH_SPRITE)
        {
            auto ft = Formatter();
            ft.Add<uint16_t>(_landRightsToolSize);
            DrawTextBasic(dpi, screenCoords - ScreenCoordsXY{ 0, 2 }, STR_LAND_TOOL_SIZE_VALUE, ft, { TextAlignment::CENTRE });
        }
        screenCoords.y = windowPos.y + window_map_widgets[WIDX_LAND_TOOL].bottom + 5;

        // People starting position (scenario editor only)
        if (widgets[WIDX_PEOPLE_STARTING_POSITION].type != WindowWidgetType::Empty)
        {
            screenCoords = windowPos
                + ScreenCoordsXY{ widgets[WIDX_PEOPLE_STARTING_POSITION].left + 12,
                                  widgets[WIDX_PEOPLE_STARTING_POSITION].top + 18 };
            GfxDrawSprite(&dpi, ImageId(SPR_6410, COLOUR_BRIGHT_RED, COLOUR_LIGHT_BROWN), screenCoords);
        }

        if (!(gScreenFlags & SCREEN_FLAGS_SCENARIO_EDITOR) && !gCheatsSandboxMode)
        {
            // Render the map legend
            if (selected_tab == PAGE_RIDES)
            {
                screenCoords = windowPos + ScreenCoordsXY{ 4, widgets[WIDX_MAP].bottom + 2 };

                static constexpr StringId _mapLabels[] = {
                    STR_MAP_RIDE,       STR_MAP_FOOD_STALL, STR_MAP_DRINK_STALL,  STR_MAP_SOUVENIR_STALL,
                    STR_MAP_INFO_KIOSK, STR_MAP_FIRST_AID,  STR_MAP_CASH_MACHINE, STR_MAP_TOILET,
                };
                static_assert(std::size(RideKeyColours) == std::size(_mapLabels));

                for (uint32_t i = 0; i < std::size(RideKeyColours); i++)
                {
                    GfxFillRect(
                        &dpi, { screenCoords + ScreenCoordsXY{ 0, 2 }, screenCoords + ScreenCoordsXY{ 6, 8 } },
                        RideKeyColours[i]);
                    DrawTextBasic(dpi, screenCoords + ScreenCoordsXY{ LIST_ROW_HEIGHT, 0 }, _mapLabels[i], {});
                    screenCoords.y += LIST_ROW_HEIGHT;
                    if (i == 3)
                    {
                        screenCoords += { 118, -(LIST_ROW_HEIGHT * 4) };
                    }
                }
            }
        }
        else if (!WidgetIsActiveTool(*this, WIDX_SET_LAND_RIGHTS))
        {
            DrawTextBasic(
                dpi, windowPos + ScreenCoordsXY{ 4, widgets[WIDX_MAP_SIZE_SPINNER_Y].top + 1 }, STR_MAP_SIZE, {},
                { colours[1] });
        }
    }

    void ResetMap()
    {
        InitMap();
        CentreMapOnViewPoint();
    }

private:
    void InitMap()
    {
        std::fill(_mapImageData.begin(), _mapImageData.end(), PALETTE_INDEX_10);
        _currentLine = 0;
    }

    void CentreMapOnViewPoint()
    {
        WindowBase* mainWindow = WindowGetMain();
        int16_t ax, bx, cx, dx;
        int16_t bp, di;

        if (mainWindow == nullptr || mainWindow->viewport == nullptr)
            return;

        auto offset = MiniMapOffsets[GetCurrentRotation()];

        // calculate centre view point of viewport and transform it to minimap coordinates

        cx = ((mainWindow->viewport->view_width >> 1) + mainWindow->viewport->viewPos.x) >> 5;
        dx = ((mainWindow->viewport->view_height >> 1) + mainWindow->viewport->viewPos.y) >> 4;
        cx += offset.x;
        dx += offset.y;

        // calculate width and height of minimap

        ax = widgets[WIDX_MAP].width() - 11;
        bx = widgets[WIDX_MAP].height() - 11;
        bp = ax;
        di = bx;

        ax >>= 1;
        bx >>= 1;
        cx = std::max(cx - ax, 0);
        dx = std::max(dx - bx, 0);

        bp = scrolls[0].h_right - bp;
        di = scrolls[0].v_bottom - di;

        if (bp < 0 && (bp - cx) < 0)
            cx = 0;

        if (di < 0 && (di - dx) < 0)
            dx = 0;

        scrolls[0].h_left = cx;
        scrolls[0].v_top = dx;
        WidgetScrollUpdateThumbs(*this, WIDX_MAP);
    }

    void IncreaseMapSize()
    {
        auto newMapSize = gMapSize;
        if (IsWidgetPressed(WIDX_MAP_SIZE_LINK) || _resizeDirection == ResizeDirection::Y)
            newMapSize.y++;
        if (IsWidgetPressed(WIDX_MAP_SIZE_LINK) || _resizeDirection == ResizeDirection::X)
            newMapSize.x++;

        auto increaseMapSizeAction = MapChangeSizeAction(newMapSize);
        GameActions::Execute(&increaseMapSizeAction);
    }

    void DecreaseMapSize()
    {
        auto newMapSize = gMapSize;
        if (IsWidgetPressed(WIDX_MAP_SIZE_LINK) || _resizeDirection == ResizeDirection::Y)
            newMapSize.y--;
        if (IsWidgetPressed(WIDX_MAP_SIZE_LINK) || _resizeDirection == ResizeDirection::X)
            newMapSize.x--;

        auto decreaseMapSizeAction = MapChangeSizeAction(newMapSize);
        GameActions::Execute(&decreaseMapSizeAction);
    }

    void SetMapPixels()
    {
        int32_t x = 0, y = 0, dx = 0, dy = 0;

        int32_t pos = (_currentLine * (MAP_WINDOW_MAP_SIZE - 1)) + MAXIMUM_MAP_SIZE_TECHNICAL - 1;
        auto destinationPosition = ScreenCoordsXY{ pos % MAP_WINDOW_MAP_SIZE, pos / MAP_WINDOW_MAP_SIZE };
        auto destination = _mapImageData.data() + (destinationPosition.y * MAP_WINDOW_MAP_SIZE) + destinationPosition.x;
        switch (GetCurrentRotation())
        {
            case 0:
                x = _currentLine * COORDS_XY_STEP;
                y = 0;
                dx = 0;
                dy = COORDS_XY_STEP;
                break;
            case 1:
                x = MAXIMUM_TILE_START_XY;
                y = _currentLine * COORDS_XY_STEP;
                dx = -COORDS_XY_STEP;
                dy = 0;
                break;
            case 2:
                x = MAXIMUM_MAP_SIZE_BIG - ((_currentLine + 1) * COORDS_XY_STEP);
                y = MAXIMUM_TILE_START_XY;
                dx = 0;
                dy = -COORDS_XY_STEP;
                break;
            case 3:
                x = 0;
                y = MAXIMUM_MAP_SIZE_BIG - ((_currentLine + 1) * COORDS_XY_STEP);
                dx = COORDS_XY_STEP;
                dy = 0;
                break;
        }

        for (int32_t i = 0; i < MAXIMUM_MAP_SIZE_TECHNICAL; i++)
        {
            if (!MapIsEdge({ x, y }))
            {
                uint16_t colour = 0;
                switch (selected_tab)
                {
                    case PAGE_PEEPS:
                        colour = GetPixelColourPeep({ x, y });
                        break;
                    case PAGE_RIDES:
                        colour = GetPixelColourRide({ x, y });
                        break;
                }
                destination[0] = (colour >> 8) & 0xFF;
                destination[1] = colour;
            }
            x += dx;
            y += dy;

            destinationPosition.x++;
            destinationPosition.y++;
            destination = _mapImageData.data() + (destinationPosition.y * MAP_WINDOW_MAP_SIZE) + destinationPosition.x;
        }
        _currentLine++;
        if (_currentLine >= MAXIMUM_MAP_SIZE_TECHNICAL)
            _currentLine = 0;
    }

    uint16_t GetPixelColourPeep(const CoordsXY& c)
    {
        auto* surfaceElement = MapGetSurfaceElementAt(c);
        if (surfaceElement == nullptr)
            return 0;

        uint16_t colour = MapColour(PALETTE_INDEX_0);
        const auto* surfaceObject = surfaceElement->GetSurfaceStyleObject();
        if (surfaceObject != nullptr)
            colour = MapColour2(surfaceObject->MapColours[0], surfaceObject->MapColours[1]);

        if (surfaceElement->GetWaterHeight() > 0)
            colour = WaterColour;

        if (!(surfaceElement->GetOwnership() & OWNERSHIP_OWNED))
            colour = MapColourUnowned(colour);

        const int32_t maxSupportedTileElementType = static_cast<int32_t>(std::size(ElementTypeAddColour));
        auto tileElement = reinterpret_cast<TileElement*>(surfaceElement);
        while (!(tileElement++)->IsLastForTile())
        {
            if (tileElement->IsGhost())
            {
                colour = MapColour(PALETTE_INDEX_21);
                break;
            }

            auto tileElementType = tileElement->GetType();
            if (EnumValue(tileElementType) >= maxSupportedTileElementType)
            {
                tileElementType = TileElementType::Surface;
            }
            colour &= ElementTypeMaskColour[EnumValue(tileElementType)];
            colour |= ElementTypeAddColour[EnumValue(tileElementType)];
        }

        return colour;
    }

    uint16_t GetPixelColourRide(const CoordsXY& c)
    {
        uint16_t colourA = 0;                           // highlight colour
        uint16_t colourB = MapColour(PALETTE_INDEX_13); // surface colour (dark grey)

        // as an improvement we could use first_element to show underground stuff?
        TileElement* tileElement = reinterpret_cast<TileElement*>(MapGetSurfaceElementAt(c));
        do
        {
            if (tileElement == nullptr)
                break;

            if (tileElement->IsGhost())
            {
                colourA = MapColour(PALETTE_INDEX_21);
                break;
            }

            switch (tileElement->GetType())
            {
                case TileElementType::Surface:
                    if (tileElement->AsSurface()->GetWaterHeight() > 0)
                        // Why is this a different water colour as above (195)?
                        colourB = MapColour(PALETTE_INDEX_194);
                    if (!(tileElement->AsSurface()->GetOwnership() & OWNERSHIP_OWNED))
                        colourB = MapColourUnowned(colourB);
                    break;
                case TileElementType::Path:
                    colourA = MapColour(PALETTE_INDEX_14); // lighter grey
                    break;
                case TileElementType::Entrance:
                {
                    if (tileElement->AsEntrance()->GetEntranceType() == ENTRANCE_TYPE_PARK_ENTRANCE)
                        break;
                    Ride* targetRide = GetRide(tileElement->AsEntrance()->GetRideIndex());
                    if (targetRide != nullptr)
                    {
                        const auto& colourKey = targetRide->GetRideTypeDescriptor().ColourKey;
                        colourA = RideKeyColours[static_cast<size_t>(colourKey)];
                    }
                    break;
                }
                case TileElementType::Track:
                {
                    Ride* targetRide = GetRide(tileElement->AsTrack()->GetRideIndex());
                    if (targetRide != nullptr)
                    {
                        const auto& colourKey = targetRide->GetRideTypeDescriptor().ColourKey;
                        colourA = RideKeyColours[static_cast<size_t>(colourKey)];
                    }

                    break;
                }
                default:
                    break;
            }
        } while (!(tileElement++)->IsLastForTile());

        if (colourA != 0)
            return colourA;

        return colourB;
    }

    void PaintPeepOverlay(DrawPixelInfo* dpi)
    {
        auto flashColour = GetGuestFlashColour();
        for (auto guest : EntityList<Guest>())
        {
            DrawMapPeepPixel(guest, flashColour, dpi);
        }
        flashColour = GetStaffFlashColour();
        for (auto staff : EntityList<Staff>())
        {
            DrawMapPeepPixel(staff, flashColour, dpi);
        }
    }

    void DrawMapPeepPixel(Peep* peep, const uint8_t flashColour, DrawPixelInfo* dpi)
    {
        if (peep->x == LOCATION_NULL)
            return;

        MapCoordsXY c = TransformToMapCoords({ peep->x, peep->y });
        auto leftTop = ScreenCoordsXY{ c.x, c.y };
        auto rightBottom = leftTop;
        uint8_t colour = DefaultPeepMapColour;
        if (EntityGetFlashing(peep))
        {
            colour = flashColour;
            // If flashing then map peep pixel size is increased (by moving left top downwards)
            if (flashColour != DefaultPeepMapColour)
            {
                leftTop.x--;
            }
        }

        GfxFillRect(dpi, { leftTop, rightBottom }, colour);
    }

    static uint8_t GetGuestFlashColour()
    {
        uint8_t colour = DefaultPeepMapColour;
        if ((gWindowMapFlashingFlags & MapFlashingFlags::FlashGuests) != 0)
        {
            colour = GuestMapColour;
            if ((gWindowMapFlashingFlags & MapFlashingFlags::SwitchColour) == 0)
                colour = GuestMapColourAlternate;
        }
        return colour;
    }

    static uint8_t GetStaffFlashColour()
    {
        uint8_t colour = DefaultPeepMapColour;
        if ((gWindowMapFlashingFlags & MapFlashingFlags::FlashStaff) != 0)
        {
            colour = StaffMapColour;
            if ((gWindowMapFlashingFlags & MapFlashingFlags::SwitchColour) == 0)
                colour = StaffMapColourAlternate;
        }
        return colour;
    }

    void PaintTrainOverlay(DrawPixelInfo* dpi)
    {
        for (auto train : TrainManager::View())
        {
            for (Vehicle* vehicle = train; vehicle != nullptr; vehicle = GetEntity<Vehicle>(vehicle->next_vehicle_on_train))
            {
                if (vehicle->x == LOCATION_NULL)
                    continue;

                MapCoordsXY c = TransformToMapCoords({ vehicle->x, vehicle->y });

                GfxFillRect(dpi, { { c.x, c.y }, { c.x, c.y } }, PALETTE_INDEX_171);
            }
        }
    }

    /**
     * The call to GfxFillRect was originally wrapped in Sub68DABD which made sure that arguments were ordered correctly,
     * but it doesn't look like it's ever necessary here so the call was removed.
     */
    void PaintHudRectangle(DrawPixelInfo* dpi)
    {
        WindowBase* mainWindow = WindowGetMain();
        if (mainWindow == nullptr)
            return;

        Viewport* mainViewport = mainWindow->viewport;
        if (mainViewport == nullptr)
            return;

        auto offset = MiniMapOffsets[GetCurrentRotation()];
        auto leftTop = ScreenCoordsXY{ (mainViewport->viewPos.x >> 5) + offset.x, (mainViewport->viewPos.y >> 4) + offset.y };
        auto rightBottom = ScreenCoordsXY{ ((mainViewport->viewPos.x + mainViewport->view_width) >> 5) + offset.x,
                                           ((mainViewport->viewPos.y + mainViewport->view_height) >> 4) + offset.y };
        auto rightTop = ScreenCoordsXY{ rightBottom.x, leftTop.y };
        auto leftBottom = ScreenCoordsXY{ leftTop.x, rightBottom.y };

        // top horizontal lines
        GfxFillRect(dpi, { leftTop, leftTop + ScreenCoordsXY{ 3, 0 } }, PALETTE_INDEX_56);
        GfxFillRect(dpi, { rightTop - ScreenCoordsXY{ 3, 0 }, rightTop }, PALETTE_INDEX_56);

        // left vertical lines
        GfxFillRect(dpi, { leftTop, leftTop + ScreenCoordsXY{ 0, 3 } }, PALETTE_INDEX_56);
        GfxFillRect(dpi, { leftBottom - ScreenCoordsXY{ 0, 3 }, leftBottom }, PALETTE_INDEX_56);

        // bottom horizontal lines
        GfxFillRect(dpi, { leftBottom, leftBottom + ScreenCoordsXY{ 3, 0 } }, PALETTE_INDEX_56);
        GfxFillRect(dpi, { rightBottom - ScreenCoordsXY{ 3, 0 }, rightBottom }, PALETTE_INDEX_56);

        // right vertical lines
        GfxFillRect(dpi, { rightTop, rightTop + ScreenCoordsXY{ 0, 3 } }, PALETTE_INDEX_56);
        GfxFillRect(dpi, { rightBottom - ScreenCoordsXY{ 0, 3 }, rightBottom }, PALETTE_INDEX_56);
    }

    void DrawTabImages(DrawPixelInfo* dpi)
    {
        // Guest tab image (animated)
        uint32_t guestTabImage = SPR_TAB_GUESTS_0;
        if (selected_tab == PAGE_PEEPS)
            guestTabImage += list_information_type / 4;

        GfxDrawSprite(
            dpi, ImageId(guestTabImage),
            windowPos + ScreenCoordsXY{ widgets[WIDX_PEOPLE_TAB].left, widgets[WIDX_PEOPLE_TAB].top });

        // Ride/stall tab image (animated)
        uint32_t rideTabImage = SPR_TAB_RIDE_0;
        if (selected_tab == PAGE_RIDES)
            rideTabImage += list_information_type / 4;

        GfxDrawSprite(
            dpi, ImageId(rideTabImage),
            windowPos + ScreenCoordsXY{ widgets[WIDX_RIDES_TAB].left, widgets[WIDX_RIDES_TAB].top });
    }

    void ShowDefaultScenarioEditorButtons()
    {
        widgets[WIDX_BUILD_PARK_ENTRANCE].type = WindowWidgetType::FlatBtn;
        widgets[WIDX_PEOPLE_STARTING_POSITION].type = WindowWidgetType::FlatBtn;
        widgets[WIDX_MAP_SIZE_SPINNER_Y].type = WindowWidgetType::Spinner;
        widgets[WIDX_MAP_SIZE_SPINNER_Y_UP].type = WindowWidgetType::Button;
        widgets[WIDX_MAP_SIZE_SPINNER_Y_DOWN].type = WindowWidgetType::Button;
        widgets[WIDX_MAP_SIZE_LINK].type = WindowWidgetType::FlatBtn;
        widgets[WIDX_MAP_SIZE_SPINNER_X].type = WindowWidgetType::Spinner;
        widgets[WIDX_MAP_SIZE_SPINNER_X_UP].type = WindowWidgetType::Button;
        widgets[WIDX_MAP_SIZE_SPINNER_X_DOWN].type = WindowWidgetType::Button;

        // Only show this in the scenario editor, even when in sandbox mode.
        if (gScreenFlags & SCREEN_FLAGS_SCENARIO_EDITOR)
            widgets[WIDX_MAP_GENERATOR].type = WindowWidgetType::Button;

        // Push width (Y) and height (X) to the common formatter arguments for the map size spinners to use
        auto ft = Formatter::Common();
        ft.Add<uint16_t>(gMapSize.y - 2);
        ft.Add<uint16_t>(gMapSize.x - 2);
    }

    void InputLandSize()
    {
        Formatter ft;
        ft.Add<int16_t>(MINIMUM_TOOL_SIZE);
        ft.Add<int16_t>(MAXIMUM_TOOL_SIZE);
        TextInputOpen(WIDX_LAND_TOOL, STR_SELECTION_SIZE, STR_ENTER_SELECTION_SIZE, ft, STR_NONE, STR_NONE, 3);
    }

    void InputMapSize(WidgetIndex callingWidget)
    {
        if (IsWidgetPressed(WIDX_MAP_SIZE_LINK))
            _resizeDirection = ResizeDirection::Both;
        else
            _resizeDirection = (callingWidget == WIDX_MAP_SIZE_SPINNER_Y) ? ResizeDirection::Y : ResizeDirection::X;

        Formatter ft;
        ft.Add<int16_t>(MINIMUM_MAP_SIZE_PRACTICAL);
        ft.Add<int16_t>(MAXIMUM_MAP_SIZE_PRACTICAL);
        TextInputOpen(callingWidget, STR_MAP_SIZE_2, STR_ENTER_MAP_SIZE, ft, STR_NONE, STR_NONE, 4);
    }

    CoordsXY ScreenToMap(ScreenCoordsXY screenCoords)
    {
        screenCoords.x = ((screenCoords.x + 8) - MAXIMUM_MAP_SIZE_TECHNICAL) / 2;
        screenCoords.y = ((screenCoords.y + 8)) / 2;
        auto location = TileCoordsXY(screenCoords.y - screenCoords.x, screenCoords.x + screenCoords.y).ToCoordsXY();

        switch (GetCurrentRotation())
        {
            case 0:
                return location;
            case 1:
                return { MAXIMUM_MAP_SIZE_BIG - 1 - location.y, location.x };
            case 2:
                return { MAXIMUM_MAP_SIZE_BIG - 1 - location.x, MAXIMUM_MAP_SIZE_BIG - 1 - location.y };
            case 3:
                return { location.y, MAXIMUM_MAP_SIZE_BIG - 1 - location.x };
        }

        return { 0, 0 }; // unreachable
    }

    MapCoordsXY TransformToMapCoords(CoordsXY c)
    {
        int32_t x = c.x, y = c.y;

        switch (GetCurrentRotation())
        {
            case 3:
                std::swap(x, y);
                x = MAXIMUM_MAP_SIZE_BIG - 1 - x;
                break;
            case 2:
                x = MAXIMUM_MAP_SIZE_BIG - 1 - x;
                y = MAXIMUM_MAP_SIZE_BIG - 1 - y;
                break;
            case 1:
                std::swap(x, y);
                y = MAXIMUM_MAP_SIZE_BIG - 1 - y;
                break;
            case 0:
                break;
        }
        x /= 32;
        y /= 32;

        return { -x + y + MAXIMUM_MAP_SIZE_TECHNICAL - 8, x + y - 8 };
    }

    uint8_t _activeTool;
    uint32_t _currentLine;
    uint16_t _landRightsToolSize;
    std::vector<uint8_t> _mapImageData;
    bool _mapWidthAndHeightLinked{ true };
    enum class ResizeDirection
    {
        Both,
        X,
        Y,
    } _resizeDirection{ ResizeDirection::Both };

    static constexpr const uint16_t RideKeyColours[] = {
        MapColour(PALETTE_INDEX_61),  // COLOUR_KEY_RIDE
        MapColour(PALETTE_INDEX_42),  // COLOUR_KEY_FOOD
        MapColour(PALETTE_INDEX_20),  // COLOUR_KEY_DRINK
        MapColour(PALETTE_INDEX_209), // COLOUR_KEY_SOUVENIR
        MapColour(PALETTE_INDEX_136), // COLOUR_KEY_KIOSK
        MapColour(PALETTE_INDEX_102), // COLOUR_KEY_FIRST_AID
        MapColour(PALETTE_INDEX_55),  // COLOUR_KEY_CASH_MACHINE
        MapColour(PALETTE_INDEX_161), // COLOUR_KEY_TOILETS
    };

    static constexpr const uint8_t DefaultPeepMapColour = PALETTE_INDEX_20;
    static constexpr const uint8_t GuestMapColour = PALETTE_INDEX_172;
    static constexpr const uint8_t GuestMapColourAlternate = PALETTE_INDEX_21;
    static constexpr const uint8_t StaffMapColour = PALETTE_INDEX_138;
    static constexpr const uint8_t StaffMapColourAlternate = PALETTE_INDEX_10;

    static constexpr const uint16_t WaterColour = MapColour(PALETTE_INDEX_195);

    static constexpr const uint16_t ElementTypeMaskColour[] = {
        0xFFFF, // TILE_ELEMENT_TYPE_SURFACE
        0x0000, // TILE_ELEMENT_TYPE_PATH
        0x00FF, // TILE_ELEMENT_TYPE_TRACK
        0xFF00, // TILE_ELEMENT_TYPE_SMALL_SCENERY
        0x0000, // TILE_ELEMENT_TYPE_ENTRANCE
        0xFFFF, // TILE_ELEMENT_TYPE_WALL
        0x0000, // TILE_ELEMENT_TYPE_LARGE_SCENERY
        0xFFFF, // TILE_ELEMENT_TYPE_BANNER
    };

    static constexpr const uint16_t ElementTypeAddColour[] = {
        MapColour(PALETTE_INDEX_0),                     // TILE_ELEMENT_TYPE_SURFACE
        MapColour(PALETTE_INDEX_17),                    // TILE_ELEMENT_TYPE_PATH
        MapColour2(PALETTE_INDEX_183, PALETTE_INDEX_0), // TILE_ELEMENT_TYPE_TRACK
        MapColour2(PALETTE_INDEX_0, PALETTE_INDEX_99),  // TILE_ELEMENT_TYPE_SMALL_SCENERY
        MapColour(PALETTE_INDEX_186),                   // TILE_ELEMENT_TYPE_ENTRANCE
        MapColour(PALETTE_INDEX_0),                     // TILE_ELEMENT_TYPE_WALL
        MapColour(PALETTE_INDEX_99),                    // TILE_ELEMENT_TYPE_LARGE_SCENERY
        MapColour(PALETTE_INDEX_0),                     // TILE_ELEMENT_TYPE_BANNER
    };
};

WindowBase* WindowMapOpen()
{
    try
    {
        WindowBase* w = WindowFocusOrCreate<MapWindow>(WindowClass::Map, 245, 259, WF_10);
        w->selected_tab = 0;
        w->list_information_type = 0;
        return w;
    }
    catch (const std::bad_alloc&)
    {
        return nullptr;
    }
}

void WindowMapReset()
{
    WindowBase* w;

    // Check if window is even opened
    w = WindowBringToFrontByClass(WindowClass::Map);
    if (w == nullptr)
    {
        return;
    }

    auto* mapWindow = static_cast<MapWindow*>(w);
    mapWindow->ResetMap();
}
