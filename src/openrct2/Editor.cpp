/*****************************************************************************
 * Copyright (c) 2014-2023 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Editor.h"

#include "Context.h"
#include "EditorObjectSelectionSession.h"
#include "FileClassifier.h"
#include "Game.h"
#include "GameState.h"
#include "OpenRCT2.h"
#include "ParkImporter.h"
#include "actions/LandBuyRightsAction.h"
#include "actions/LandSetRightsAction.h"
#include "actions/ResultWithMessage.h"
#include "audio/audio.h"
#include "core/Path.hpp"
#include "entity/EntityList.h"
#include "entity/EntityRegistry.h"
#include "entity/Guest.h"
#include "entity/PatrolArea.h"
#include "entity/Staff.h"
#include "interface/Viewport.h"
#include "interface/Window_internal.h"
#include "localisation/Localisation.h"
#include "localisation/LocalisationService.h"
#include "management/Finance.h"
#include "management/NewsItem.h"
#include "object/DefaultObjects.h"
#include "object/ObjectManager.h"
#include "object/ObjectRepository.h"
#include "rct1/RCT1.h"
#include "scenario/Scenario.h"
#include "ui/UiContext.h"
#include "ui/WindowManager.h"
#include "util/Util.h"
#include "windows/Intent.h"
#include "world/Climate.h"
#include "world/Entrance.h"
#include "world/Footpath.h"
#include "world/Park.h"
#include "world/Scenery.h"

#include <algorithm>
#include <array>
#include <vector>

using namespace OpenRCT2;

EditorStep gEditorStep;

namespace Editor
{
    static std::array<std::vector<uint8_t>, EnumValue(ObjectType::Count)> _editorSelectedObjectFlags;

    static void ConvertSaveToScenarioCallback(int32_t result, const utf8* path);
    static void SetAllLandOwned();
    static void FinaliseMainView();
    static bool ReadS4OrS6(const char* path);
    static bool ReadPark(const char* path);
    static void ClearMapForEditing(bool fromSave);

    static void ObjectListLoad()
    {
        auto* context = GetContext();

        // Unload objects first, the repository is re-populated which owns the objects.
        auto& objectManager = context->GetObjectManager();
        objectManager.UnloadAll();

        // Scan objects if necessary
        const auto& localisationService = context->GetLocalisationService();
        auto& objectRepository = context->GetObjectRepository();
        objectRepository.LoadOrConstruct(localisationService.GetCurrentLanguage());

        Audio::LoadAudioObjects();

        // Reset loaded objects to just defaults
        // Load minimum required objects (like surface and edge)
        for (const auto& entry : MinimumRequiredObjects)
        {
            objectManager.LoadObject(entry);
        }
    }

    static WindowBase* OpenEditorWindows()
    {
        auto* main = ContextOpenWindow(WindowClass::MainWindow);
        ContextOpenWindow(WindowClass::TopToolbar);
        ContextOpenWindowView(WV_EDITOR_BOTTOM_TOOLBAR);
        return main;
    }

    /**
     *
     *  rct2: 0x0066FFE1
     */
    void Load()
    {
        OpenRCT2::Audio::StopAll();
        ObjectListLoad();
        OpenRCT2::GetContext()->GetGameState()->InitAll(DEFAULT_MAP_SIZE);
        gScreenFlags = SCREEN_FLAGS_SCENARIO_EDITOR;
        gEditorStep = EditorStep::ObjectSelection;
        gParkFlags |= PARK_FLAGS_SHOW_REAL_GUEST_NAMES;
        gScenarioCategory = SCENARIO_CATEGORY_OTHER;
        ViewportInitAll();
        WindowBase* mainWindow = OpenEditorWindows();
        mainWindow->SetLocation(TileCoordsXYZ{ 75, 75, 14 }.ToCoordsXYZ());
        LoadPalette();
        gScreenAge = 0;
        gScenarioName = LanguageGetString(STR_MY_NEW_SCENARIO);
    }

    /**
     *
     *  rct2: 0x00672781
     */
    void ConvertSaveToScenario()
    {
        ToolCancel();
        auto intent = Intent(WindowClass::Loadsave);
        intent.PutExtra(INTENT_EXTRA_LOADSAVE_TYPE, LOADSAVETYPE_LOAD | LOADSAVETYPE_GAME);
        intent.PutExtra(INTENT_EXTRA_CALLBACK, reinterpret_cast<void*>(ConvertSaveToScenarioCallback));
        ContextOpenIntent(&intent);
    }

    static void ConvertSaveToScenarioCallback(int32_t result, const utf8* path)
    {
        if (result != MODAL_RESULT_OK)
        {
            return;
        }

        if (!GetContext()->LoadParkFromFile(path))
        {
            return;
        }

        ScenarioReset();

        gScreenFlags = SCREEN_FLAGS_SCENARIO_EDITOR;
        gEditorStep = EditorStep::ObjectiveSelection;
        gScenarioCategory = SCENARIO_CATEGORY_OTHER;
        ViewportInitAll();
        OpenEditorWindows();
        FinaliseMainView();
        gScreenAge = 0;
    }

    /**
     *
     *  rct2: 0x00672957
     */
    void LoadTrackDesigner()
    {
        OpenRCT2::Audio::StopAll();
        gScreenFlags = SCREEN_FLAGS_TRACK_DESIGNER;
        gScreenAge = 0;

        ObjectManagerUnloadAllObjects();
        ObjectListLoad();
        OpenRCT2::GetContext()->GetGameState()->InitAll(DEFAULT_MAP_SIZE);
        SetAllLandOwned();
        gEditorStep = EditorStep::ObjectSelection;
        ViewportInitAll();
        WindowBase* mainWindow = OpenEditorWindows();
        mainWindow->SetLocation(TileCoordsXYZ{ 75, 75, 14 }.ToCoordsXYZ());
        LoadPalette();
    }

    /**
     *
     *  rct2: 0x006729FD
     */
    void LoadTrackManager()
    {
        OpenRCT2::Audio::StopAll();
        gScreenFlags = SCREEN_FLAGS_TRACK_MANAGER;
        gScreenAge = 0;

        ObjectManagerUnloadAllObjects();
        ObjectListLoad();
        OpenRCT2::GetContext()->GetGameState()->InitAll(DEFAULT_MAP_SIZE);
        SetAllLandOwned();
        gEditorStep = EditorStep::ObjectSelection;
        ViewportInitAll();
        WindowBase* mainWindow = OpenEditorWindows();
        mainWindow->SetLocation(TileCoordsXYZ{ 75, 75, 14 }.ToCoordsXYZ());
        LoadPalette();
    }

    /**
     *
     *  rct2: 0x0068ABEC
     */
    static void SetAllLandOwned()
    {
        MapRange range = { 2 * COORDS_XY_STEP, 2 * COORDS_XY_STEP, (gMapSize.x - 3) * COORDS_XY_STEP,
                           (gMapSize.y - 3) * COORDS_XY_STEP };
        auto landSetRightsAction = LandSetRightsAction(range, LandSetRightSetting::SetForSale);
        landSetRightsAction.SetFlags(GAME_COMMAND_FLAG_NO_SPEND);
        GameActions::Execute(&landSetRightsAction);

        auto landBuyRightsAction = LandBuyRightsAction(range, LandBuyRightSetting::BuyLand);
        landBuyRightsAction.SetFlags(GAME_COMMAND_FLAG_NO_SPEND);
        GameActions::Execute(&landBuyRightsAction);
    }

    /**
     *
     *  rct2: 0x006758C0
     */
    bool LoadLandscape(const utf8* path)
    {
        // #4996: Make sure the object selection window closes here to prevent unload objects
        //        after we have loaded a new park.
        WindowCloseAll();

        auto extension = GetFileExtensionType(path);
        switch (extension)
        {
            case FileExtension::SC6:
            case FileExtension::SV6:
            case FileExtension::SC4:
            case FileExtension::SV4:
                return ReadS4OrS6(path);
            case FileExtension::PARK:
                return ReadPark(path);
            default:
                return false;
        }
    }

    static void AfterLoadCleanup(bool loadedFromSave)
    {
        ClearMapForEditing(loadedFromSave);

        gEditorStep = EditorStep::LandscapeEditor;
        gScreenAge = 0;
        gScreenFlags = SCREEN_FLAGS_SCENARIO_EDITOR;
        ViewportInitAll();
        OpenEditorWindows();
        FinaliseMainView();
    }

    /**
     *
     *  rct2: 0x006758FE
     */
    static bool ReadS4OrS6(const char* path)
    {
        auto extensionS = Path::GetExtension(path);
        const char* extension = extensionS.c_str();
        auto loadedFromSave = false;
        const auto loadSuccess = GetContext()->LoadParkFromFile(path);
        if (!loadSuccess)
            return false;

        if (_stricmp(extension, ".sv4") == 0 || _stricmp(extension, ".sv6") == 0 || _stricmp(extension, ".sv7") == 0)
        {
            loadedFromSave = true;
        }

        AfterLoadCleanup(loadedFromSave);
        return true;
    }

    static bool ReadPark(const char* path)
    {
        try
        {
            auto context = GetContext();
            auto& objManager = context->GetObjectManager();
            auto importer = ParkImporter::CreateParkFile(context->GetObjectRepository());
            auto loadResult = importer->Load(path);
            objManager.LoadObjects(loadResult.RequiredObjects);
            importer->Import();

            AfterLoadCleanup(true);
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    static void ClearMapForEditing(bool fromSave)
    {
        MapRemoveAllRides();
        UnlinkAllRideBanners();

        RideInitAll();

        //
        for (auto* guest : EntityList<Guest>())
        {
            guest->SetName({});
        }
        for (auto* staff : EntityList<Staff>())
        {
            staff->SetName({});
        }

        ResetAllEntities();
        UpdateConsolidatedPatrolAreas();
        gNumGuestsInPark = 0;
        gNumGuestsHeadingForPark = 0;
        gNumGuestsInParkLastWeek = 0;
        gGuestChangeModifier = 0;
        if (fromSave)
        {
            gParkFlags |= PARK_FLAGS_NO_MONEY;

            if (gParkEntranceFee == 0)
            {
                gParkFlags |= PARK_FLAGS_PARK_FREE_ENTRY;
            }
            else
            {
                gParkFlags &= ~PARK_FLAGS_PARK_FREE_ENTRY;
            }

            gParkFlags &= ~PARK_FLAGS_SPRITES_INITIALISED;

            gGuestInitialCash = std::clamp(gGuestInitialCash, 10.00_GBP, MAX_ENTRANCE_FEE);

            gInitialCash = std::min<money64>(gInitialCash, 100000);
            FinanceResetCashToInitial();

            gBankLoan = std::clamp<money64>(gBankLoan, 0.00_GBP, 5000000.00_GBP);

            gMaxBankLoan = std::clamp<money64>(gMaxBankLoan, 0.00_GBP, 5000000.00_GBP);

            gBankLoanInterestRate = std::clamp<uint8_t>(gBankLoanInterestRate, 5, MaxBankLoanInterestRate);
        }

        ClimateReset(gClimate);

        News::InitQueue();
    }

    /**
     *
     *  rct2: 0x0067009A
     */
    void OpenWindowsForCurrentStep()
    {
        if (!(gScreenFlags & SCREEN_FLAGS_EDITOR))
        {
            return;
        }

        switch (gEditorStep)
        {
            case EditorStep::ObjectSelection:
                if (WindowFindByClass(WindowClass::EditorObjectSelection) != nullptr)
                {
                    return;
                }

                if (WindowFindByClass(WindowClass::InstallTrack) != nullptr)
                {
                    return;
                }

                if (gScreenFlags & SCREEN_FLAGS_TRACK_MANAGER)
                {
                    ObjectManagerUnloadAllObjects();
                }

                ContextOpenWindow(WindowClass::EditorObjectSelection);
                break;
            case EditorStep::InventionsListSetUp:
                if (WindowFindByClass(WindowClass::EditorInventionList) != nullptr)
                {
                    return;
                }

                ContextOpenWindow(WindowClass::EditorInventionList);
                break;
            case EditorStep::OptionsSelection:
                if (WindowFindByClass(WindowClass::EditorScenarioOptions) != nullptr)
                {
                    return;
                }

                ContextOpenWindow(WindowClass::EditorScenarioOptions);
                break;
            case EditorStep::ObjectiveSelection:
                if (WindowFindByClass(WindowClass::EditorObjectiveOptions) != nullptr)
                {
                    return;
                }

                ContextOpenWindow(WindowClass::EditorObjectiveOptions);
                break;
            case EditorStep::LandscapeEditor:
            case EditorStep::SaveScenario:
            case EditorStep::RollercoasterDesigner:
            case EditorStep::DesignsManager:
            case EditorStep::Invalid:
                break;
        }
    }

    static void FinaliseMainView()
    {
        auto windowManager = GetContext()->GetUiContext()->GetWindowManager();
        windowManager->SetMainView(gSavedView, gSavedViewZoom, gSavedViewRotation);

        ResetAllSpriteQuadrantPlacements();
        ScenerySetDefaultPlacementConfiguration();

        windowManager->BroadcastIntent(Intent(INTENT_ACTION_REFRESH_NEW_RIDES));

        gWindowUpdateTicks = 0;
        LoadPalette();

        windowManager->BroadcastIntent(Intent(INTENT_ACTION_CLEAR_TILE_INSPECTOR_CLIPBOARD));
    }

    /**
     *
     *  rct2: 0x006AB9B8
     */
    std::pair<ObjectType, StringId> CheckObjectSelection()
    {
        bool isTrackDesignerManager = gScreenFlags & (SCREEN_FLAGS_TRACK_DESIGNER | SCREEN_FLAGS_TRACK_MANAGER);

        if (!isTrackDesignerManager)
        {
            if (!EditorCheckObjectGroupAtLeastOneSurfaceSelected(false))
            {
                return { ObjectType::FootpathSurface, STR_AT_LEAST_ONE_FOOTPATH_NON_QUEUE_SURFACE_OBJECT_MUST_BE_SELECTED };
            }
            if (!EditorCheckObjectGroupAtLeastOneSurfaceSelected(true))
            {
                return { ObjectType::FootpathSurface, STR_AT_LEAST_ONE_FOOTPATH_QUEUE_SURFACE_OBJECT_MUST_BE_SELECTED };
            }
            if (!EditorCheckObjectGroupAtLeastOneSelected(ObjectType::FootpathRailings))
            {
                return { ObjectType::FootpathRailings, STR_AT_LEAST_ONE_FOOTPATH_RAILING_OBJECT_MUST_BE_SELECTED };
            }
        }

        if (!EditorCheckObjectGroupAtLeastOneSelected(ObjectType::Ride))
        {
            return { ObjectType::Ride, STR_AT_LEAST_ONE_RIDE_OBJECT_MUST_BE_SELECTED };
        }
        if (!EditorCheckObjectGroupAtLeastOneSelected(ObjectType::Station))
        {
            return { ObjectType::Station, STR_AT_LEAST_ONE_STATION_OBJECT_MUST_BE_SELECTED };
        }

        if (!EditorCheckObjectGroupAtLeastOneSelected(ObjectType::TerrainSurface))
        {
            return { ObjectType::TerrainSurface, STR_AT_LEAST_ONE_TERRAIN_SURFACE_OBJECT_MUST_BE_SELECTED };
        }
        if (!EditorCheckObjectGroupAtLeastOneSelected(ObjectType::TerrainEdge))
        {
            return { ObjectType::TerrainEdge, STR_AT_LEAST_ONE_TERRAIN_EDGE_OBJECT_MUST_BE_SELECTED };
        }

        if (!isTrackDesignerManager)
        {
            if (!EditorCheckObjectGroupAtLeastOneSelected(ObjectType::ParkEntrance))
            {
                return { ObjectType::ParkEntrance, STR_PARK_ENTRANCE_TYPE_MUST_BE_SELECTED };
            }

            if (!EditorCheckObjectGroupAtLeastOneSelected(ObjectType::Water))
            {
                return { ObjectType::Water, STR_WATER_TYPE_MUST_BE_SELECTED };
            }
        }

        return { ObjectType::None, STR_NONE };
    }

    /**
     *
     *  rct2: 0x0066FEAC
     */
    ResultWithMessage CheckPark()
    {
        int32_t parkSize = ParkCalculateSize();
        if (parkSize == 0)
        {
            return { false, STR_PARK_MUST_OWN_SOME_LAND };
        }

        if (gParkEntrances.empty())
        {
            return { false, STR_NO_PARK_ENTRANCES };
        }

        for (const auto& parkEntrance : gParkEntrances)
        {
            int32_t direction = DirectionReverse(parkEntrance.direction);

            switch (FootpathIsConnectedToMapEdge(parkEntrance, direction, 0))
            {
                case FOOTPATH_SEARCH_NOT_FOUND:
                    return { false, STR_PARK_ENTRANCE_WRONG_DIRECTION_OR_NO_PATH };
                case FOOTPATH_SEARCH_INCOMPLETE:
                case FOOTPATH_SEARCH_TOO_COMPLEX:
                    return { false, STR_PARK_ENTRANCE_PATH_INCOMPLETE_OR_COMPLEX };
                case FOOTPATH_SEARCH_SUCCESS:
                    // Run the search again and unown the path
                    FootpathIsConnectedToMapEdge(parkEntrance, direction, (1 << 5));
                    break;
            }
        }

        if (gPeepSpawns.empty())
        {
            return { false, STR_PEEP_SPAWNS_NOT_SET };
        }

        return { true, STR_NONE };
    }

    uint8_t GetSelectedObjectFlags(ObjectType objectType, size_t index)
    {
        uint8_t result = 0;
        auto& list = _editorSelectedObjectFlags[EnumValue(objectType)];
        if (list.size() > index)
        {
            result = list[index];
        }
        return result;
    }

    void ClearSelectedObject(ObjectType objectType, size_t index, uint32_t flags)
    {
        auto& list = _editorSelectedObjectFlags[EnumValue(objectType)];
        if (list.size() <= index)
        {
            list.resize(index + 1);
        }
        list[index] &= ~flags;
    }

    void SetSelectedObject(ObjectType objectType, size_t index, uint32_t flags)
    {
        if (index != OBJECT_ENTRY_INDEX_NULL)
        {
            assert(static_cast<int32_t>(objectType) < object_entry_group_counts[EnumValue(ObjectType::Paths)]);
            auto& list = _editorSelectedObjectFlags[EnumValue(objectType)];
            if (list.size() <= index)
            {
                list.resize(index + 1);
            }
            list[index] |= flags;
        }
    }
} // namespace Editor

void EditorOpenWindowsForCurrentStep()
{
    Editor::OpenWindowsForCurrentStep();
}
