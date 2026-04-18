#include <pch.h>

#include <core/render.h>

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>
#include <misc/imgui_utility.h>
#include <game/rtech/utils/bsp/bspflags.h>

#include <core/render/dx.h>
#include <core/window.h>
#include <core/input/input.h>

#include <core/filehandling/export.h>

#include <game/rtech/cpakfile.h>
#include <game/rtech/assets/model.h>
#include <game/rtech/assets/texture.h>
#include <game/rtech/assets/settings.h>
#include <core/render/preview/preview.h>

#include "render/ui/styles.h"
#include <core/fonts/codicons.h>

#include <regex>
#include <directxtex/DirectXTex.h>

extern CDXParentHandler* g_dxHandler;
extern std::atomic<uint32_t> g_maxConcurrentThreadCount;
extern ExportSettings_t g_ExportSettings;

PreviewSettings_t g_PreviewSettings { .previewCullDistance = PREVIEW_CULL_DEFAULT, .previewMovementSpeed = PREVIEW_SPEED_DEFAULT };

CPreviewDrawData g_currentPreviewDrawData;

std::atomic<bool> inJobAction = false;

enum AssetColumn_t
{
    AC_Type,
    AC_Name,
    AC_GUID,
    AC_File,

    _AC_COUNT,
};

struct AssetCompare_t
{
    const ImGuiTableSortSpecs* sortSpecs;

    // TODO: get rid of pak-specific stuff
    bool operator() (const CGlobalAssetData::AssetLookup_t& a, const CGlobalAssetData::AssetLookup_t& b) const
    {
        const CAsset* assetA = a.m_asset;
        const CAsset* assetB = b.m_asset;

        for (int n = 0; n < sortSpecs->SpecsCount; n++)
        {
            // Here we identify columns using the ColumnUserID value that we ourselves passed to TableSetupColumn()
            // We could also choose to identify columns based on their index (sort_spec->ColumnIndex), which is simpler!
            const ImGuiTableColumnSortSpecs* sort_spec = &sortSpecs->Specs[n];
            __int64 delta = 0;
            switch (sort_spec->ColumnUserID)
            {
            case AssetColumn_t::AC_Type:   delta = static_cast<int64_t>(_byteswap_ulong(assetA->GetAssetType())) - _byteswap_ulong(assetB->GetAssetType());                        break; // no overflow please
            case AssetColumn_t::AC_Name:   delta = _stricmp(assetA->GetAssetName().c_str(), assetB->GetAssetName().c_str());                        break; // no overflow please
            case AssetColumn_t::AC_GUID:   delta = assetA->GetAssetGUID() - assetB->GetAssetGUID();     break;
            case AssetColumn_t::AC_File:   delta = _stricmp(assetA->GetContainerFileName().c_str(), assetB->GetContainerFileName().c_str());     break;
            default: IM_ASSERT(0); break;
            }
            if (delta > 0)
                return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? false : true;
            if (delta < 0)
                return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? true : false;
        }

        return (static_cast<int64_t>(assetA->GetAssetType()) - assetB->GetAssetType()) > 0;
    }
};

void ColouredTextForAssetType(const CAsset* const asset)
{
    bool colouredText = false;

    switch (asset->GetAssetContainerType())
    {
    case CAsset::ContainerType::PAK:
    case CAsset::ContainerType::AUDIO:
    case CAsset::ContainerType::MDL:
    case CAsset::ContainerType::BP_PAK:
    {
        const AssetType_t assetType = static_cast<AssetType_t>(asset->GetAssetType());
        if (s_AssetTypeColours.contains(assetType))
        {
            colouredText = true;

            const Color4& col = s_AssetTypeColours.at(assetType);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(col.r, col.g, col.b, col.a));
        }

        ImGui::Text("%s %s", fourCCToString(asset->GetAssetType()).c_str(), asset->GetAssetVersion().ToString().c_str());
        break;
    }
    [[unlikely]] default:
    {
        ImGui::TextUnformatted("unimpl");
        break;
    }
    }

    if (colouredText)
        ImGui::PopStyleColor();
}

// [ Preview ] Render the asset dependencies table in the preview window for the provided asset
void PreviewWnd_AssetDepsTbl(CAsset* asset)
{
    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);

    std::vector<AssetGuid_t> dependencies;
    pakAsset->getDependencies(dependencies);

    constexpr ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable
        | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoBordersInBody
        | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

    const ImVec2 outerSize = ImVec2(0.f, ImGui::GetTextLineHeightWithSpacing() * 12.f);

    constexpr int numColumns = 5;

    if (ImGui::TreeNodeEx("Asset Dependencies", ImGuiTreeNodeFlags_SpanAvailWidth))
    {
        if (ImGui::BeginTable("Assets", numColumns, tableFlags, outerSize))
        {
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 0.f, 0);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 0.f, 1);
            ImGui::TableSetupColumn("Pak", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 0.f, 2);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 0.f, 3);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 0.f, 4);
            ImGui::TableSetupScrollFreeze(1, 1);

            ImGui::TableHeadersRow();

            for (int d = 0; d < dependencies.size(); ++d)
            {
                AssetGuid_t guid = dependencies[d];
                CAsset* depAsset = g_assetData.FindAssetByGUID<CPakAsset>(guid.guid);

#if _DEBUG
                // [rika]: cannot access depAsset if nullptr
                if (depAsset)
                    assert(depAsset->GetAssetContainerType() == CAsset::ContainerType::PAK);
#endif // _DEBUG

                ImGui::PushID(d);

                ImGui::TableNextRow(ImGuiTableRowFlags_None, 0.f);

                // Asset Type
                if (ImGui::TableSetColumnIndex(0))
                {
                    ImGui::AlignTextToFramePadding();
                    if (depAsset)
                        ColouredTextForAssetType(depAsset);
                    else
                        ImGui::TextUnformatted("n/a");
                }

                // Asset GUID
                if (ImGui::TableSetColumnIndex(1))
                {
                    ImGui::AlignTextToFramePadding();
                    if (depAsset)
                        ImGui::TextUnformatted(depAsset->GetAssetName().c_str());
                    else
                        ImGui::Text("%016llX", guid.guid);
                }

                // Asset Container Name (e.g. common.rpak, general_stream.mstr)
                if (ImGui::TableSetColumnIndex(2))
                {
                    ImGui::AlignTextToFramePadding();
                    if (!depAsset)
                        ImGui::TextUnformatted("n/a");
                    else
                        ImGui::TextUnformatted(depAsset->GetContainerFileName().c_str());
                }

                // Export Status
                if (ImGui::TableSetColumnIndex(3))
                {
                    ImGui::AlignTextToFramePadding();
                    if (depAsset)
                    {
                        if (depAsset->GetExportedStatus())
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 1.f, 1.f, 1.f));
                            ImGui::TextUnformatted("Exported");
                        }
                        else
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 1.f, 0.f, 1.f));
                            ImGui::TextUnformatted("Loaded");
                        }
                    }
                    else
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, Styles::TEXTCOL_NOT_LOADED);
                        ImGui::TextUnformatted("Not Loaded");
                    }

                    ImGui::PopStyleColor();
                }

                if (ImGui::TableSetColumnIndex(4))
                {
                    ImGui::AlignTextToFramePadding();
                    if (!depAsset)
                        ImGui::BeginDisabled();

                    if (ImGui::Button("Export"))
                        CThread(HandleExportBindingForAsset, depAsset, g_ExportSettings.exportAssetDeps).detach();

                    if (!depAsset)
                        ImGui::EndDisabled();
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        ImGui::TreePop();
    }
}

void PreviewWnd_AssetParentsTbl(CAsset* asset)
{
    const uint64_t assetGuid = asset->GetAssetGUID();

    // Collect all PAK assets that have this asset's GUID in their dependency list.
    std::vector<CPakAsset*> parents;
    for (auto& lookup : g_assetData.v_assets)
    {
        CAsset* candidate = lookup.m_asset;
        if (candidate == asset)
            continue;

        if (candidate->GetAssetContainerType() != CAsset::ContainerType::PAK)
            continue;

        CPakAsset* pakCandidate = static_cast<CPakAsset*>(candidate);

        std::vector<AssetGuid_t> deps;
        pakCandidate->getDependencies(deps);

        for (const AssetGuid_t& dep : deps)
        {
            if (dep.guid == assetGuid)
            {
                parents.push_back(pakCandidate);
                break;
            }
        }
    }

    constexpr ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable
        | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoBordersInBody
        | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

    const ImVec2 outerSize = ImVec2(0.f, ImGui::GetTextLineHeightWithSpacing() * 12.f);

    constexpr int numColumns = 5;

    if (ImGui::TreeNodeEx("Parent Assets", ImGuiTreeNodeFlags_SpanAvailWidth))
    {
        if (parents.empty())
        {
            ImGui::TextUnformatted("No loaded assets reference this asset.");
        }
        else if (ImGui::BeginTable("ParentAssets", numColumns, tableFlags, outerSize))
        {
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 0.f, 0);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 0.f, 1);
            ImGui::TableSetupColumn("Pak", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 0.f, 2);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 0.f, 3);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 0.f, 4);
            ImGui::TableSetupScrollFreeze(1, 1);

            ImGui::TableHeadersRow();

            for (int p = 0; p < static_cast<int>(parents.size()); ++p)
            {
                CPakAsset* parentAsset = parents[p];

                ImGui::PushID(p);

                ImGui::TableNextRow(ImGuiTableRowFlags_None, 0.f);

                // Asset Type
                if (ImGui::TableSetColumnIndex(0))
                {
                    ImGui::AlignTextToFramePadding();
                    ColouredTextForAssetType(parentAsset);
                }

                // Asset Name
                if (ImGui::TableSetColumnIndex(1))
                {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(parentAsset->GetAssetName().c_str());
                }

                // Container (pak) file name
                if (ImGui::TableSetColumnIndex(2))
                {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(parentAsset->GetContainerFileName().c_str());
                }

                // Export Status
                if (ImGui::TableSetColumnIndex(3))
                {
                    ImGui::AlignTextToFramePadding();
                    if (parentAsset->GetExportedStatus())
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 1.f, 1.f, 1.f));
                        ImGui::TextUnformatted("Exported");
                    }
                    else
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 1.f, 0.f, 1.f));
                        ImGui::TextUnformatted("Loaded");
                    }
                    ImGui::PopStyleColor();
                }

                // Export button
                if (ImGui::TableSetColumnIndex(4))
                {
                    ImGui::AlignTextToFramePadding();
                    if (ImGui::Button("Export"))
                        CThread(HandleExportBindingForAsset, parentAsset, g_ExportSettings.exportAssetDeps).detach();
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        ImGui::TreePop();
    }
}

void SettingsWnd_Draw(CUIState* uiState)
{
    constexpr uint32_t minThreads = 1u;

    ImGui::SetNextWindowSize(ImVec2(0.f, 500.f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", &uiState->settingsWindowVisible, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse))
    {
#if !defined(_DEBUG) && !defined(NO_LIBCURL)
        // ===============================================================================================================
        ImGui::SeparatorText("General");

        ImGui::Checkbox("Check for updates", &UtilsConfig->checkForUpdates);
        ImGui::SameLine();
        ImGuiExt::HelpMarker("RSX will check for updates against the GitHub repository when opened.\nIf there is a new update available, a message will be displayed on the RSX welcome dialog box");
#endif
        
        // ===============================================================================================================
		ImGui::SeparatorText("Search");

        ImGui::Checkbox("Search by GUID", &FilterConfig->searchByGuid);
        ImGui::SameLine();
        ImGuiExt::HelpMarker("When enabled, the search box will match assets whose GUID (computed from the asset name) equals the entered hexadecimal or decimal value.\nThis is in addition to the normal name filter.");

        ImGui::SeparatorText("Export");

        ImGui::Checkbox("Export full asset paths", &g_ExportSettings.exportPathsFull);
        ImGui::SameLine();
        ImGuiExt::HelpMarker("Enables exporting of assets to their full path within the export directory, as shown by the listed asset names.\nWhen disabled, assets will be exported into the root-level of a folder named after the asset's type (e.g. \"material/\",\"ui_image/\").");

        ImGui::Checkbox("Export asset dependencies", &g_ExportSettings.exportAssetDeps);
        ImGui::SameLine();
        ImGuiExt::HelpMarker("Enables exporting of all dependencies that are associated with any asset that is being exported.");

        ImGui::Checkbox("Disable CacheDB loading", &g_ExportSettings.disableCachedNames);
        ImGui::SameLine();
        ImGuiExt::HelpMarker("Disables loading names from the cache file. The cache will still be updated, but RSX will not display any cached data");

        // texture settings
        ImGui::SeparatorText("Export (Textures)");

        ImGui::Combo("Material Texture Naming", reinterpret_cast<int*>(&g_ExportSettings.exportTextureNameSetting), s_TextureExportNameSetting, static_cast<int>(ARRAYSIZE(s_TextureExportNameSetting)));
        ImGui::SameLine();
        ImGuiExt::HelpMarker("Naming scheme for exporting textures via materials options are as follows:\nGUID: exports only using the asset's GUID as a name.\nReal: exports texture using a real name (asset name or guid if no name).\nText: exports the texture with a text name always, generating one if there is none provided.\nSemantic: exports with a generated name all the time, useful for models.");

        ImGui::Combo("Normal Recalculation", reinterpret_cast<int*>(&g_ExportSettings.exportNormalRecalcSetting), s_NormalExportRecalcSetting, static_cast<int>(ARRAYSIZE(s_NormalExportRecalcSetting)));
        ImGui::SameLine();
        ImGuiExt::HelpMarker("None: exports the normal as it is stored.\nDirectX: exports with a generated blue channel.\nOpenGL: exports with a generated blue channel and inverts the green channel.");

        ImGui::Checkbox("Export Material Textures", &g_ExportSettings.exportMaterialTextures);
        ImGui::SameLine();
        ImGuiExt::HelpMarker("Enables exporting of all textures that are associated with any material asset that is being exported.");

        // model settings
        ImGui::SeparatorText("Export (Models)");

        ImGui::Checkbox("Export Sequences", &g_ExportSettings.exportRigSequences);
        ImGui::SameLine();
        ImGuiExt::HelpMarker("Enables exporting of all animation sequences that are associated with any rig or model asset that is being exported.");

        ImGui::Checkbox("Export Anim Data (.asqd)", &g_ExportSettings.exportSeqAnimData);
        ImGui::SameLine();
        ImGuiExt::HelpMarker("Enables automatic exporting of ASQD files along with RSEQ.");

        ImGui::Checkbox("Export Skin", &g_ExportSettings.exportModelSkin);
        ImGui::SameLine();
        ImGuiExt::HelpMarker("Enables exporting a model with the previewed skin.");

        ImGui::Checkbox("Truncate Materials", &g_ExportSettings.exportModelMatsTruncated);
        ImGui::SameLine();
        ImGuiExt::HelpMarker("Truncates material names on SMD.");

        ImGui::Checkbox("Enable QCI Files", &g_ExportSettings.exportQCIFiles);
        ImGui::SameLine();
        ImGuiExt::HelpMarker("QC file will be split into multiple include files.");

        ImGui::PushItemWidth(48.0f);
        ImGui::InputScalar("##QCTargetMajor", ImGuiDataType_U16, reinterpret_cast<uint16_t*>(&g_ExportSettings.qcMajorVersion), nullptr, nullptr, "%u", ImGuiInputTextFlags_CharsDecimal);
        ImGui::SameLine();
        ImGui::InputScalar("##QCTargetMinor", ImGuiDataType_U16, reinterpret_cast<uint16_t*>(&g_ExportSettings.qcMinorVersion), nullptr, nullptr, "%u", ImGuiInputTextFlags_CharsDecimal);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::Text("QC Target Version");
        ImGui::SameLine();
        ImGuiExt::HelpMarker("Desired version for QC files to be compatible with.");

        // physics settings
        ImGui::InputInt("Physics contents filter", reinterpret_cast<int*>(&g_ExportSettings.exportPhysicsContentsFilter), 1, 100, ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::SameLine();
        ImGuiExt::HelpMarker("Filter physics meshes in or out based on selected contents.");

        ImGui::Checkbox("Physics contents filter exclusive", &g_ExportSettings.exportPhysicsFilterExclusive);
        ImGui::SameLine();
        ImGuiExt::HelpMarker("Exclude physics meshes containing any of the specified contents.");

        ImGui::Checkbox("Physics contents filter require all", &g_ExportSettings.exportPhysicsFilterAND);
        ImGui::SameLine();
        ImGuiExt::HelpMarker("Filter only physics meshes containing all specified contents.");

        // ===============================================================================================================
        ImGui::SeparatorText("Parsing");

        ImGui::Combo("Compression Level", reinterpret_cast<int*>(&UtilsConfig->compressionLevel), s_CompressionLevelSetting, static_cast<int>(ARRAYSIZE(s_CompressionLevelSetting)));
        ImGui::SameLine();
        ImGuiExt::HelpMarker("Specifies the compression level used when storing parsed assets in memory.\nWARNING: Modify only if you know what you're doing; otherwise, you may run out of memory.\nNone: no compression.\nSuper Fast: Fastest level with the lowest compression ratio.\nVery Fast: Standard setting; fastest level with a decent compression ratio.\nFast: Fastest level with a good compression ratio.\nNormal: Standard LZ speed with the highest compression ratio.");

        ImGui::SliderScalar("Parse Threads", ImGuiDataType_U32, &UtilsConfig->parseThreadCount, &minThreads, reinterpret_cast<int*>(&g_maxConcurrentThreadCount));
        ImGui::SameLine();
        ImGuiExt::HelpMarker("The number of CPU threads that will be used for loading files.\n\nIn general, the higher the number, the faster RSX will be able to load the selected files.");

        ImGui::SliderScalar("Export Threads", ImGuiDataType_U32, &UtilsConfig->exportThreadCount, &minThreads, reinterpret_cast<int*>(&g_maxConcurrentThreadCount));
        ImGui::SameLine();
        ImGuiExt::HelpMarker("The number of CPU threads that will be used for exporting assets.\n\nA higher number of threads will usually make RSX export assets more quickly, however the increased disk usage may cause decreased performance.");

        // ===============================================================================================================
        ImGui::SeparatorText("Preview");

        if (ImGui::SliderFloat("Cull Distance", &g_PreviewSettings.previewCullDistance, PREVIEW_CULL_MIN, PREVIEW_CULL_MAX))
            g_dxHandler->UpdateProjectionMatrix();

        ImGui::SameLine();
        ImGuiExt::HelpMarker("Distance at which render of 3D objects will stop.");

        // ===============================================================================================================
        ImGui::SeparatorText("Export Formats");

        for (auto& [fourCC, binding] : g_assetData.m_assetTypeBindings)
        {
            if (binding.e.exportSettingArr)
            {
                ImGui::Combo(fourCCToString(fourCC).c_str(), &binding.e.exportSetting, binding.e.exportSettingArr, static_cast<int>(binding.e.exportSettingArrSize));
            }
        }
    }
    ImGui::End();
}
extern void ItemflavWnd_Draw(CUIState*);
extern void LogWnd_Draw(CUIState*);

static std::deque<CAsset*> s_selectedAssets;
static std::vector<CGlobalAssetData::AssetLookup_t> s_filteredAssets;
static CAsset* s_prevRenderInfoAsset = nullptr;

void ApplySelectionRequests(ImGuiMultiSelectIO* ms_io, std::deque<CAsset*>& selectedAssets, std::vector<CGlobalAssetData::AssetLookup_t>& pakAssets)
{
    for (ImGuiSelectionRequest& req : ms_io->Requests)
    {
        if (req.Type == ImGuiSelectionRequestType_SetAll) {
            selectedAssets.clear();
            if (req.Selected) {
                for (int n = 0; n < pakAssets.size(); n++)
                {
                    selectedAssets.insert(selectedAssets.end(), pakAssets[n].m_asset);
                }
            }
        }

        if (req.Type == ImGuiSelectionRequestType_SetRange)
        {
            for (int n = (int)req.RangeFirstItem; n <= (int)req.RangeLastItem; n++)
            {
                auto iter = std::find(selectedAssets.begin(), selectedAssets.end(), pakAssets[n].m_asset);
                const bool isSelected = iter != selectedAssets.end();

                if (!isSelected && req.Selected) selectedAssets.insert(selectedAssets.end(), pakAssets[n].m_asset);
                if (isSelected && !req.Selected) selectedAssets.erase(iter);
            }
        }
    }
}

void MainWnd_MenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        CUIState& uiState = g_dxHandler->GetUIState();
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open"))
            {
                if (!inJobAction)
                {
                    s_selectedAssets.clear();
                    s_filteredAssets.clear();
                    s_prevRenderInfoAsset = nullptr;
                    g_assetData.ClearAssetData();
                    uiState.ClearAssetData();

                    // We kinda leak the thread here but it's okay, we want it to keep executing.
                    CThread(HandleOpenFileDialog, g_dxHandler->GetWindowHandle()).detach();
                }
            }

            if (ImGui::MenuItem("Unload Files"))
            {
                if (!inJobAction)
                {
                    if (g_assetData.v_assets.size() > 0)
                        g_assetData.Log_Info(nullptr, "Unloaded %lld asset%s from %lld container file%s", g_assetData.v_assets.size(), g_assetData.v_assets.size() == 1 ? "" : "s", g_assetData.v_assetContainers.size(), g_assetData.v_assetContainers.size() == 1 ? "" : "s");

                    s_selectedAssets.clear();
                    s_filteredAssets.clear();
                    s_prevRenderInfoAsset = nullptr;
                    g_assetData.ClearAssetData();
                    uiState.ClearAssetData();
                }
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Settings"))
                uiState.ShowSettingsWindow(true);

#if defined(HAS_ITEMFLAV_WINDOW)
            if (ImGui::MenuItem("Itemflavors"))
                uiState.ShowItemflavWindow(true);
#endif

            if (ImGui::MenuItem("Logs"))
                uiState.ShowLogWindow(true);

            ImGui::EndMenu();
        }
        
        const std::pair<int, int> errorCounts = g_assetData.m_logErrorListInfo.GetPair();
        
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 5.f);
        ImGuiExt::IconText(ICON_CI_WARNING, ImVec4(1.f, 1.f, 0.f, 1.f));  ImGui::SameLine();
        ImGui::Text("%i", errorCounts.first); ImGui::SameLine();
        ImGuiExt::IconText(ICON_CI_ERROR, ImVec4(1.f, 0.f, 0.f, 1.f)); ImGui::SameLine();
        ImGui::Text("%i", errorCounts.second);

#if _DEBUG
        IMGUI_RIGHT_ALIGN_FOR_TEXT("Avg 1.000 ms/frame (100.0 FPS)"); // [rexx]: i hate this actually

        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("Avg %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
#endif
        ImGui::EndMainMenuBar();
    }
}

void MainWnd_WelcomeBox()
{
    if (!inJobAction && g_assetData.v_assetContainers.empty())
    {
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        ImGui::SetNextWindowSize(ImVec2(600, 0), ImGuiCond_Always);

        // [Dialog] Welcome Message
        if (ImGui::Begin("reSource Xtractor", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImGui::PushTextWrapPos();
            ImGui::TextUnformatted(RSX_WELCOME_MESSAGE);
            ImGui::PopTextWrapPos();

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5.f);

            if (ImGui::Button("Open File..."))
            {
                // Reset selected asset to avoid crash.
                s_selectedAssets.clear();
                s_filteredAssets.clear();
                s_prevRenderInfoAsset = nullptr;
                g_assetData.ClearAssetData();

                // We kinda leak the thread here but it's okay, we want it to keep executing.
                CThread(HandleOpenFileDialog, g_dxHandler->GetWindowHandle()).detach();
            }

            CUIState& uiState = g_dxHandler->GetUIState();

            if (UtilsConfig->checkForUpdates && uiState.newVersionType)
            {
                ImGui::Separator();
                ImGui::Text("There's a new update available for RSX! Download the latest %s version from", uiState.newVersionType);
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 5.f);
                ImGui::TextLinkOpenURL("here!", uiState.newVersionReleaseInfo.htmlUrl.c_str());
            }

            ImGui::End();
        }
    }
}

static ImVec2 s_previousAvailableSizeForPreview(-1, -1);

static const unsigned char s_regexPngData[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x14, 0x08, 0x06, 0x00, 0x00, 0x00, 0x8d, 0x89, 0x1d,
    0x0d, 0x00, 0x00, 0x00, 0xdd, 0x49, 0x44, 0x41, 0x54, 0x78, 0x01, 0xed, 0xc1, 0xb1, 0x4d, 0x42,
    0x01, 0x18, 0x06, 0xc0, 0xfb, 0x09, 0x03, 0xbc, 0xc2, 0xc6, 0x4a, 0x9c, 0x40, 0xdd, 0x00, 0x37,
    0xc0, 0x0d, 0x64, 0x02, 0xd9, 0x40, 0x47, 0x80, 0x01, 0x4c, 0x70, 0x01, 0x95, 0x0d, 0x18, 0x81,
    0xc4, 0xce, 0xc6, 0xb7, 0x81, 0x32, 0xc1, 0xa7, 0x09, 0x26, 0xa2, 0x09, 0xf1, 0x11, 0x5b, 0xef,
    0xfc, 0xfb, 0x55, 0x92, 0x26, 0x49, 0x63, 0x87, 0x9e, 0xfd, 0x4d, 0x30, 0xb7, 0x43, 0xe9, 0x20,
    0x49, 0x83, 0x6b, 0xcc, 0x70, 0x89, 0x21, 0x46, 0xb8, 0xc6, 0xac, 0xaa, 0x5a, 0x9f, 0xfa, 0xba,
    0x69, 0x70, 0x81, 0x09, 0x96, 0x38, 0xc6, 0x0b, 0xd6, 0x98, 0xd9, 0x52, 0xf6, 0x90, 0xe4, 0x16,
    0x63, 0x1b, 0x0b, 0x8c, 0xab, 0xea, 0xcd, 0x96, 0xd2, 0x41, 0x92, 0x06, 0x0f, 0x18, 0xe2, 0x09,
    0x07, 0x38, 0xc4, 0x12, 0xe3, 0xaa, 0x6a, 0x7d, 0xea, 0xe9, 0xa6, 0xc1, 0x1a, 0xe7, 0xb8, 0xc7,
    0x33, 0xce, 0xb0, 0xf6, 0x57, 0x49, 0x6e, 0x92, 0x2c, 0xed, 0xd0, 0xf7, 0x43, 0x92, 0x11, 0xae,
    0x6c, 0xb4, 0x98, 0x55, 0xd5, 0xca, 0x97, 0x29, 0xe6, 0x76, 0x28, 0x5b, 0x92, 0x34, 0x78, 0xc5,
    0x02, 0x2b, 0x8c, 0xb0, 0xaa, 0xaa, 0x4b, 0x1d, 0xf5, 0x7c, 0x77, 0x6a, 0x63, 0x5a, 0x55, 0x37,
    0x78, 0xc4, 0xc0, 0x1e, 0xfa, 0xbe, 0x6b, 0x6d, 0x9c, 0xe4, 0x03, 0x8e, 0xec, 0xa9, 0x67, 0x4b,
    0x55, 0xb5, 0xb8, 0xc3, 0x14, 0x4b, 0x5c, 0x60, 0xee, 0xaf, 0x92, 0x0c, 0x92, 0x0c, 0xfd, 0xeb,
    0xe2, 0x1d, 0x67, 0xbe, 0x4a, 0x08, 0xff, 0x68, 0x84, 0xad, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
    0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};
static const unsigned int s_regexPngSize = 278;

static ID3D11ShaderResourceView* s_regexIconSRV = nullptr;

static void EnsureRegexIcon()
{
    if (s_regexIconSRV)
        return;

    DirectX::ScratchImage scratch;
    if (FAILED(DirectX::LoadFromWICMemory(s_regexPngData, s_regexPngSize, DirectX::WIC_FLAGS_NONE, nullptr, scratch)))
        return;

    extern CDXParentHandler* g_dxHandler;
    DirectX::CreateShaderResourceView(g_dxHandler->GetDevice(),
        scratch.GetImages(), scratch.GetImageCount(), scratch.GetMetadata(),
        &s_regexIconSRV);
}

struct RegexCache_t
{
    std::string  pattern;
    std::regex   compiled;
    bool         valid = false;
};
static RegexCache_t s_regexCache;

using AssetList = std::vector<CGlobalAssetData::AssetLookup_t>;

static bool                    s_filterPendingRebuild = false;
static std::atomic<uint32_t>   s_filterGen{ 0 };
static std::atomic<AssetList*> s_filterReady{ nullptr };

void HandleRenderFrame()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // create a docking area across the entire viewport
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        const ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");

        // If the dockspace hasn't been created yet
        if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr)
        {
            DockBuilder(dockspaceId)
                .Window("Scene", true)
                .DockLeft(0.25f)
                    .Window("Asset List")
                    .Done()
                .DockRight(0.25f)
                    .Window("Asset Info");
        }
        //ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::DockSpaceOverViewport(dockspaceId, NULL, ImGuiDockNodeFlags_PassthruCentralNode, 0);
    }

    // while ImGui is using keyboard input, we should not accept any keyboard input, but we should also clear all  
    // existing key states, as otherwise we can end up moving forever (until ImGui loses focus) in model preview if
    // the movement keys are held when ImGui starts capturing keyboard
    if (ImGui::GetIO().WantCaptureKeyboard)
        g_pInput->ClearKeyStates();

    g_pInput->Frame(ImGui::GetIO().DeltaTime);

    g_dxHandler->GetCamera()->Move(ImGui::GetIO().DeltaTime);

    CUIState& uiState = g_dxHandler->GetUIState();

#if defined(DEBUG_IMGUI_DEMO)
    ImGui::ShowDemoWindow();
#endif

    MainWnd_MenuBar();

    MainWnd_WelcomeBox();

    CDXDrawData* previewDrawData = nullptr;

    static size_t prevAssetCount = g_assetData.v_assets.size();

    const bool shouldPopulateAssetWindows = !inJobAction && !g_assetData.v_assetContainers.empty();

    if (ImGui::Begin("Asset List", nullptr, ImGuiWindowFlags_MenuBar) && shouldPopulateAssetWindows)
    {

        std::vector<CGlobalAssetData::AssetLookup_t>& pakAssets = FilterConfig->textFilter.IsActive() ? s_filteredAssets : g_assetData.v_assets;

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Export"))
            {
                const bool multipleAssetsSelected = s_selectedAssets.size() > 1;

                if (ImGui::Selectable(multipleAssetsSelected ? "Export selected assets" : "Export selected asset"))
                {
                    if (!s_selectedAssets.empty())
                    {
                        std::deque<CAsset*> cpyAssets;
                        cpyAssets.insert(cpyAssets.end(), s_selectedAssets.begin(), s_selectedAssets.end());
                        CThread(HandlePakAssetExportList, std::move(cpyAssets), g_ExportSettings.exportAssetDeps).detach();
                        s_selectedAssets.clear();
                    }
                }

                if (ImGui::Selectable("Export all for selected type", false, multipleAssetsSelected ? ImGuiSelectableFlags_Disabled : 0))
                {
                    if (s_selectedAssets.size() == 1)
                    {
                        const uint32_t desiredType = s_selectedAssets[0]->GetAssetType();
                        auto allAssetsOfDesiredType = pakAssets | std::ranges::views::filter([desiredType](const CGlobalAssetData::AssetLookup_t& a)
                            {
                                return a.m_asset->GetAssetType() == desiredType;
                            });

                        std::vector<CGlobalAssetData::AssetLookup_t> allAssets(allAssetsOfDesiredType.begin(), allAssetsOfDesiredType.end());
                        CThread(HandleExportSelectedAssetType, std::move(allAssets), g_ExportSettings.exportAssetDeps).detach();
                        s_selectedAssets.clear();
                    }
                }

                if (ImGui::Selectable("Export all for selected pak", false, multipleAssetsSelected ? ImGuiSelectableFlags_Disabled : 0))
                {
                    if (s_selectedAssets.size() == 1 && s_selectedAssets[0]->GetAssetContainerType() == CAsset::ContainerType::PAK)
                    {
                        const CPakFile* desiredPak = s_selectedAssets[0]->GetContainerFile<const CPakFile>();
                        auto allAssetsOfDesiredType = pakAssets | std::ranges::views::filter([desiredPak](const CGlobalAssetData::AssetLookup_t& a)
                            {
                                return a.m_asset->GetAssetContainerType() == CAsset::ContainerType::PAK && a.m_asset->GetContainerFile<const CPakFile>() == desiredPak;
                            });

                        std::vector<CGlobalAssetData::AssetLookup_t> allAssets(allAssetsOfDesiredType.begin(), allAssetsOfDesiredType.end());
                        CThread(HandleExportSelectedAssetType, std::move(allAssets), g_ExportSettings.exportAssetDeps).detach();
                        s_selectedAssets.clear();
                    }
                }

                if (ImGui::Selectable("Export all for selected pak and type", false, multipleAssetsSelected ? ImGuiSelectableFlags_Disabled : 0))
                {
                    if (s_selectedAssets.size() == 1 && s_selectedAssets[0]->GetAssetContainerType() == CAsset::ContainerType::PAK)
                    {
                        const CPakFile* desiredPak = s_selectedAssets[0]->GetContainerFile<const CPakFile>();
                        const uint32_t desiredType = s_selectedAssets[0]->GetAssetType();
                        auto allAssetsOfDesiredType = pakAssets | std::ranges::views::filter([desiredPak, desiredType](const CGlobalAssetData::AssetLookup_t& a)
                            {
                                return a.m_asset->GetAssetContainerType() == CAsset::ContainerType::PAK
                                    && a.m_asset->GetContainerFile<CPakFile>() == desiredPak
                                    && a.m_asset->GetAssetType() == desiredType;
                            });

                        std::vector<CGlobalAssetData::AssetLookup_t> allAssets(allAssetsOfDesiredType.begin(), allAssetsOfDesiredType.end());
                        CThread(HandleExportSelectedAssetType, std::move(allAssets), g_ExportSettings.exportAssetDeps).detach();
                        s_selectedAssets.clear();
                    }
                }

                if (ImGui::Selectable("Export all"))
                    CThread(HandleExportAllPakAssets, &pakAssets, g_ExportSettings.exportAssetDeps).detach();

                // Exports the names of all assets in the currently shown filtered asset list (i.e., search results)
                if (ImGui::Selectable("Export list of asset names..."))
                    CThread(HandleListExportPakAssets, g_dxHandler->GetWindowHandle(), &pakAssets).detach();

                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        EnsureRegexIcon();

        const ImVec2 framePad = ImGui::GetStyle().FramePadding;
        const float iconBtnSize = ImGui::GetFrameHeight();
        const float imgSize = iconBtnSize - framePad.y * 2.f;
        const float btnTotalWidth = imgSize + framePad.x * 2.f;
        const float inputWidth = ImGui::GetContentRegionAvail().x - btnTotalWidth;

        const float rowY = ImGui::GetCursorPosY();
        bool filterChanged = FilterConfig->textFilter.Draw("##Filter", inputWidth);

        ImGui::SameLine(0.f, 0.f);
        ImGui::SetCursorPosY(rowY);
        {
            const bool regexOn = FilterConfig->searchByRegex;

            if (regexOn)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.51f, 0.78f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.36f, 0.61f, 0.88f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.45f, 0.72f, 1.00f));
            }

            const ImVec4 tint = regexOn ? ImVec4(1.f, 1.f, 1.f, 1.f) : ImVec4(0.5f, 0.5f, 0.5f, 1.f);
            const ImTextureID iconTex = reinterpret_cast<ImTextureID>(s_regexIconSRV);

            if (ImGui::ImageButton("##RegexToggle", iconTex, ImVec2(imgSize, imgSize), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), tint))
            {
                FilterConfig->searchByRegex = !FilterConfig->searchByRegex;
                filterChanged = true;
                ImGui::MarkIniSettingsDirty();
            }

            if (regexOn)
                ImGui::PopStyleColor(3);

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && ImGui::BeginTooltip())
            {
                ImGui::TextUnformatted(regexOn ? "Regex search: ON\nClick to disable" : "Regex search: OFF\nClick to enable");
                ImGui::EndTooltip();
            }
        }

        if (filterChanged || prevAssetCount != g_assetData.v_assets.size())
        {
            s_filterPendingRebuild = true;
        }

        if (s_filterPendingRebuild)
        {

            s_filterPendingRebuild = false;

            if (!FilterConfig->textFilter.IsActive())
            {
                s_filteredAssets.clear();
            }
            else
            {
                const uint32_t myGen = ++s_filterGen;
                const bool useRegex = FilterConfig->searchByRegex;
                const bool useGuid = FilterConfig->searchByGuid;
                const std::string rawInput = FilterConfig->textFilter.inputBuf;

                bool       regexValid = false;
                std::regex regexForWorker;
                if (useRegex)
                {
                    if (rawInput != s_regexCache.pattern)
                    {
                        s_regexCache.pattern = rawInput;
                        s_regexCache.valid = false;
                        try
                        {
                            s_regexCache.compiled = std::regex(rawInput,
                                std::regex_constants::icase | std::regex_constants::optimize);
                            s_regexCache.valid = true;
                        }
                        catch (const std::regex_error&) {}
                    }
                    regexValid = s_regexCache.valid;
                    regexForWorker = s_regexCache.compiled;
                }

                uint64_t legacyGuid = 0; bool legacyGuidParsed = false;
                std::string guidSubstr; // stripped, uppercased hex substring to search for
                uint64_t nameHashedGuid = 0; bool nameHashedGuidValid = false;
                {
                    const char* p = rawInput.c_str();
                    char* end = nullptr;
                    const uint64_t g = strtoull(p, &end, 0);
                    const bool isNumeric = (end == p + rawInput.size()) && !rawInput.empty();
                    if (!useRegex && !useGuid) { legacyGuidParsed = isNumeric; legacyGuid = g; }

                    if (useGuid && !rawInput.empty())
                    {
                        // Strip optional 0x/0X prefix, then uppercase for substring matching
                        const std::string stripped = (rawInput.size() > 2 && rawInput[0] == '0' && (rawInput[1] == 'x' || rawInput[1] == 'X'))
                            ? rawInput.substr(2) : rawInput;

                        // Only use as GUID substring if every character is a valid hex digit
                        const bool allHex = !stripped.empty() && std::all_of(stripped.begin(), stripped.end(), [](unsigned char c) {
                            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
                            });
                        if (allHex)
                        {
                            guidSubstr.resize(stripped.size());
                            std::transform(stripped.begin(), stripped.end(), guidSubstr.begin(), [](unsigned char c) -> char { return static_cast<char>(::toupper(c)); });
                        }

                        // Also hash the input as an asset name/path so full names can be searched
                        std::string normalizedInput = rawInput;
                        std::replace(normalizedInput.begin(), normalizedInput.end(), '/', '\\');
                        nameHashedGuid = RTech::StringToGuid(normalizedInput.c_str());
                        nameHashedGuidValid = true;
                    }
                }

                std::vector<CGlobalAssetData::AssetLookup_t> snapshot = g_assetData.v_assets;

                std::thread([myGen, useRegex, regexValid, regexForWorker = std::move(regexForWorker), useGuid,
                    guidSubstr, nameHashedGuidValid, nameHashedGuid,
                    legacyGuidParsed, legacyGuid, snapshot = std::move(snapshot), textFilter = FilterConfig->textFilter]() mutable
                    {
                        AssetList* results = new AssetList();
                        results->reserve(snapshot.size() / 4);

                        for (auto& it : snapshot)
                        {
                            if (s_filterGen.load(std::memory_order_relaxed) != myGen)
                            {
                                delete results;
                                return;
                            }

                            const std::string& name = it.m_asset->GetAssetName();
                            bool matched = false;

                            if (useRegex)
                            {
                                if (regexValid)
                                    matched = std::regex_search(name, regexForWorker);
                            }
                            else
                            {
                                matched = textFilter.PassFilter(name.c_str());
                            }

                            // GUID substring search: format asset GUID as 16 hex chars and check if guidSubstr appears in it
                            if (!matched && useGuid && !guidSubstr.empty())
                            {
                                const std::string assetGuidStr = std::format("{:016X}", it.m_asset->GetAssetGUID());
                                matched = assetGuidStr.find(guidSubstr) != std::string::npos;
                            }
                            // Also match by hashing the input as an asset name/path
                            if (!matched && useGuid && nameHashedGuidValid)
                                matched = (nameHashedGuid == it.m_asset->GetAssetGUID());
                            if (!matched && !useRegex && !useGuid && legacyGuidParsed)
                                matched = (legacyGuid == RTech::StringToGuid(name.c_str()));

                            if (matched)
                                results->push_back(it);
                        }

                        AssetList* prev = s_filterReady.exchange(results, std::memory_order_acq_rel);
                        delete prev;
                    }).detach();
            }
        }

        if (AssetList* ready = s_filterReady.exchange(nullptr, std::memory_order_acq_rel))
        {
            s_filteredAssets = std::move(*ready);
            delete ready;
        }
		
        constexpr int numColumns = AssetColumn_t::_AC_COUNT;
        if (ImGui::BeginTable("Assets", numColumns, ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_BordersOuterV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable))
        {
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 0, AssetColumn_t::AC_Type);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, 0, AssetColumn_t::AC_Name);
            ImGui::TableSetupColumn("GUID", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultHide, 0, AssetColumn_t::AC_GUID);
            ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch, 0, AssetColumn_t::AC_File);

            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs();

            if (sortSpecs && sortSpecs->SpecsDirty && pakAssets.size() > 1)
            {
                std::sort(pakAssets.begin(), pakAssets.end(), AssetCompare_t(sortSpecs));
                sortSpecs->SpecsDirty = false;
            }

            ImGuiMultiSelectFlags flags = ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_BoxSelect1d;
            ImGuiMultiSelectIO* ms_io = ImGui::BeginMultiSelect(flags, static_cast<int>(s_selectedAssets.size()), static_cast<int>(pakAssets.size()));

            ApplySelectionRequests(ms_io, s_selectedAssets, pakAssets);

            // arbitrary large number that will never happen
            static unsigned int lastSelectionSize = UINT32_MAX;

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(pakAssets.size()));
            while (clipper.Step())
            {
                for (int rowNum = clipper.DisplayStart; rowNum < clipper.DisplayEnd; rowNum++)
                {
                    CAsset* const asset = pakAssets[rowNum].m_asset;

                    // previously this was GUID_pakCRC but realistically the pak filename also works instead of the crc (though it may be slower for lookup?)
                    ImGui::PushID(std::format("{:X}_{}", asset->GetAssetGUID(), asset->GetContainerFileName()).c_str());

                    ImGui::TableNextRow();

                    if (ImGui::TableSetColumnIndex(AssetColumn_t::AC_Type))
                    {
                        ColouredTextForAssetType(asset);

                        auto typeBinding = g_assetData.m_assetTypeBindings.find(asset->GetAssetType());
                        if (typeBinding != g_assetData.m_assetTypeBindings.end())
                            ImGuiExt::Tooltip(typeBinding->second.name);
                    }

                    if (ImGui::TableSetColumnIndex(AssetColumn_t::AC_Name))
                    {
                        const bool isSelected = std::find(s_selectedAssets.begin(), s_selectedAssets.end(), asset) != s_selectedAssets.end();
                        ImGui::SetNextItemSelectionUserData(rowNum);
                        if (ImGui::Selectable(asset->GetAssetName().c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
                        {
                            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            {
                                // if the double clicked asset is not in the list, add it
                                // this is a very strange case but the only way you can export multiple assets by double clicking is by
                                // holding shift or ctrl while double clicking, which may cause the hovered asset to be deselected before export
                                if (!isSelected)
                                    s_selectedAssets.insert(s_selectedAssets.end(), asset);

                                CThread(HandlePakAssetExportList, std::move(s_selectedAssets), g_ExportSettings.exportAssetDeps).detach();

                                s_selectedAssets.clear();
                            }
                        }
                    }

                    if (ImGui::TableSetColumnIndex(AssetColumn_t::AC_GUID))
                        ImGui::Text("%016llX", asset->GetAssetGUID());

                    if (ImGui::TableSetColumnIndex(AssetColumn_t::AC_File))
                        ImGui::TextUnformatted(asset->GetContainerFileName().c_str());

                    ImGui::PopID();
                }
            }
            ms_io = ImGui::EndMultiSelect();

            ApplySelectionRequests(ms_io, s_selectedAssets, pakAssets);

            ImGui::EndTable();
        }
    }
    ImGui::End();

    // This window must be drawn before "Scene", as the scene relies on previewDrawData already being set from here
    if (ImGui::Begin("Asset Info", nullptr, ImGuiWindowFlags_MenuBar) && shouldPopulateAssetWindows)
    {
        CAsset* const firstAsset = s_selectedAssets.empty() ? nullptr : *s_selectedAssets.begin();
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::BeginMenu("Export"))
                {
                    // File > Export > Quick Export
                    // Option to "quickly" export the asset to the exported_files directory
                    // in the format defined by the "Export Options" menu.
                    if (ImGui::MenuItem("Quick Export"))
                        CThread(HandleExportBindingForAsset, std::move(firstAsset), g_ExportSettings.exportAssetDeps).detach();

                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        const std::string assetName = !firstAsset ? "(none)" : firstAsset->GetAssetName();
        const std::string assetGuidStr = !firstAsset ? "(none)" : std::format("{:X}", firstAsset->GetAssetGUID());

        ImGuiExt::ConstTextInputLeft("Name", assetName.c_str(), 50);
        ImGuiExt::ConstTextInputLeft("GUID", assetGuidStr.c_str(), 50);

        // Dependency information only exists for PAK assets
        if (firstAsset && firstAsset->GetAssetContainerType() == CAsset::ContainerType::PAK)
        {
            const PakAsset_t* const pakAsset = static_cast<CPakAsset*>(firstAsset)->data();

            ImGui::Text("Number of Dependencies: %hu", pakAsset->dependenciesCount);
            ImGui::SameLine();
            ImGuiExt::HelpMarker("This is the number of assets in the same .rpak file as this asset that are required by this asset");

            ImGui::Text("Number of Dependent Assets: %hu", pakAsset->dependentsCount);
            ImGui::SameLine();
            ImGuiExt::HelpMarker("This is the number of assets in the same .rpak file as this asset that use this asset");

            PreviewWnd_AssetDepsTbl(firstAsset);
            PreviewWnd_AssetParentsTbl(firstAsset);
        }

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5.f);

        ImGui::Separator();

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5.f);
        ImGui::Dummy(ImVec2(0, 0)); // Required just in case there are no ImGui elements in the below preview call
        if (firstAsset)
        {
            const uint32_t type = firstAsset->GetAssetType();
            if (auto it = g_assetData.m_assetTypeBindings.find(type); it != g_assetData.m_assetTypeBindings.end())
            {
                if (it->second.previewFunc)
                {
                    // First frame is a special case, we wanna reset some settings for the preview function.
                    const bool firstFrameForAsset = firstAsset != s_prevRenderInfoAsset;

                    previewDrawData = reinterpret_cast<CDXDrawData*>(it->second.previewFunc(static_cast<CPakAsset*>(firstAsset), firstFrameForAsset));
                    s_prevRenderInfoAsset = firstAsset;

                }
                else ImGui::Text("Asset type '%s' does not currently support Asset Preview.", fourCCToString(type).c_str());
            }
            else ImGui::Text("Asset type '%s' is not currently supported.", fourCCToString(type).c_str());
        }
        else ImGui::TextUnformatted("No asset selected.");
    }
    ImGui::End();

    // The "scene" preview window must always be in the center.
    // Setting NoMove seems to be the best way to stop it from being undocked
    if (ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoMove))
    {
        const ImVec2 avail = ImGui::GetContentRegionAvail();

        if (avail != s_previousAvailableSizeForPreview || !g_dxHandler->GetPreviewRTV())
        {
            g_dxHandler->CleanupForPreviewResize();
            g_dxHandler->CreateViewForSceneWindow(static_cast<uint16_t>(avail.x), static_cast<uint16_t>(avail.y));
        }

        s_previousAvailableSizeForPreview = avail;

        if (previewDrawData)
        {
            switch (previewDrawData->dataType)
            {
            case CDXDrawData::DrawDataType_e::MODEL:
                Preview_Model(previewDrawData, ImGui::GetIO().DeltaTime);
                break;
            case CDXDrawData::DrawDataType_e::TEXTURE:
                Preview_Texture(previewDrawData, ImGui::GetIO().DeltaTime);
                break;
            default:
            {
                assertm(0, "Invalid preview draw data type. Expected either MODEL or TEXTURE");
                break;
            }
            }
        }
    }
    ImGui::End();

    if (uiState.settingsWindowVisible)
        SettingsWnd_Draw(&uiState);

#if defined(HAS_ITEMFLAV_WINDOW)
    if (uiState.itemflavWindowVisible)
        ItemflavWnd_Draw(&uiState);
#endif

    if (uiState.logWindowVisible)
        LogWnd_Draw(&uiState);

    g_pImGuiHandler->HandleProgressBar();

    ImGui::Render();

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    prevAssetCount = g_assetData.v_assets.size(); // <-- ADD THIS LINE

    ID3D11DeviceContext* const ctx = g_dxHandler->GetDeviceContext();

    ID3D11RenderTargetView* const mainView = g_dxHandler->GetMainView();
    static constexpr float clear_color_with_alpha[4] = { 0.01f, 0.01f, 0.01f, 1.00f };

    ctx->OMSetRenderTargets(1, &mainView, g_dxHandler->GetDepthStencilView());
    ctx->ClearRenderTargetView(mainView, clear_color_with_alpha);
    ctx->ClearDepthStencilView(g_dxHandler->GetDepthStencilView(), D3D11_CLEAR_DEPTH, 1, 0);

    LONG width = 0ul;
    LONG height = 0ul;
    g_dxHandler->GetWindowSize(&width, &height);

    const D3D11_VIEWPORT vp = {
        0, 0,
        static_cast<float>(width),
        static_cast<float>(height),
        0, 1
    };

    ctx->RSSetViewports(1u, &vp);

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    g_dxHandler->GetSwapChain()->Present(1u, 0u);
}