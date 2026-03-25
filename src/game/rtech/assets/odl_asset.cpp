#include "pch.h"
#include "odl_asset.h"
#include "odl_pak.h"

#include <game/rtech/cpakfile.h>
#include <game/rtech/utils/utils.h>
#include <thirdparty/imgui/imgui.h>
#include <core/render/dx.h>

#if defined(HAS_ODL_ASSET)

ODLAsset::ODLAsset(const ODLAssetHeader_t* const header) :
    pakAssetGuid(header->odlPakAssetGuid),
    originalAssetGuid(header->originalAssetGuid),
    placeholderAssetGuid(header->placeholderAssetGuid),
    originalAssetName(header->assetName),
    originalAssetPakName(nullptr)
{
    CAsset* odlPakAsset = g_assetData.FindAssetByGUID(header->odlPakAssetGuid);

    if (odlPakAsset)
    {
        CPakAsset* odlPakPakAsset = reinterpret_cast<CPakAsset*>(odlPakAsset);
        ODLPakHeader_t* odlPakHeader = reinterpret_cast<ODLPakHeader_t*>(odlPakPakAsset->header());

        originalAssetPakName = odlPakHeader->pakName;
    }
};

void LoadODLAsset(CAssetContainer* container, CAsset* asset)
{
    CPakFile* const pak = static_cast<CPakFile* const>(container);
    CPakAsset* const pakAsset = static_cast<CPakAsset* const>(asset);

    UNUSED(pak);
    UNUSED(pakAsset);

    const auto header = reinterpret_cast<const ODLAssetHeader_t*>(pakAsset->header());

    ODLAsset* odlAsset = new ODLAsset(header);

    pakAsset->SetAssetName(header->assetName, true);
    pakAsset->setExtraData(odlAsset);
}

static bool ExportODLAsset(CAsset* const asset, const int setting)
{
    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);
    UNUSED(setting);

    ODLAsset* odlAsset = pakAsset->extraData<ODLAsset*>();

    std::filesystem::path exportPath = g_ExportSettings.GetExportDirectory();
    std::filesystem::path odlPath = asset->GetAssetName();

    exportPath.append("odl_asset/");
    exportPath.append(odlPath.parent_path().string());

    if (!CreateDirectories(exportPath))
    {
        assertm(false, "Failed to create asset type directory.");
        return false;
    }

    exportPath.append(odlPath.filename().string());
    exportPath.replace_extension(".json");

    StreamIO out;
    if (!out.open(exportPath.string(), eStreamIOMode::Write))
    {
        assertm(false, "Failed to open file for write.");
        return false;
    }

    const char* originalPakName = odlAsset->GetPakName();
    const std::string pakName = originalPakName ? originalPakName : "(unknown)";

    std::string stringStream;

    stringStream +=
        "{\n"
        "\t\"odlGuid\": \"" + std::format("{:x}", pakAsset->GetAssetGUID()) + "\",\n"
        "\t\"name\": \"" + std::string(odlAsset->GetOriginalAssetName()) + "\",\n"
        "\t\"pak\": \"" + std::string(pakName) + "\"\n"
        "}";

    out.write(stringStream.c_str(), stringStream.length());
    
    return true;
}

void* PreviewODLAsset(CAsset* const asset, const bool _firstFrame)
{
    UNUSED(_firstFrame); // at the moment we don't care about the odl asset's first preview frame
                         // in the future, this will be used to load the odl pak when this asset is being previewed

    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);

    ODLAsset* odlAsset = pakAsset->extraData<ODLAsset*>();

    ODLAssetHeader_t* header = reinterpret_cast<ODLAssetHeader_t*>(pakAsset->header());

    CAsset* loadedAsset = g_assetData.FindAssetByGUID(header->originalAssetGuid);

    CDXDrawData* drawData = NULL;
    if (loadedAsset && loadedAsset->GetPostLoadStatus())
    {
        if (auto it = g_assetData.m_assetTypeBindings.find(loadedAsset->GetAssetType()); it != g_assetData.m_assetTypeBindings.end())
        {
            if (it->second.previewFunc)
            {
                static CAsset* lastPreviewedODLAsset = NULL;

                // First frame is a special case, we wanna reset some settings for the preview function.
                const bool firstFrameForAsset = loadedAsset != lastPreviewedODLAsset;

                drawData = reinterpret_cast<CDXDrawData*>(it->second.previewFunc(static_cast<CPakAsset*>(loadedAsset), firstFrameForAsset));
                lastPreviewedODLAsset = loadedAsset;
            }
            else
            {
                ImGui::Text("Asset type '%s' does not currently support Asset Preview.", fourCCToString(loadedAsset->GetAssetType()).c_str());
            }
        }
        
    }
    else
        ImGui::Text("Can't preview this asset: You must also select '%s' when choosing which files to open!", odlAsset->GetPakName());


    return drawData;
}
#endif

void InitODLAssetType()
{
#if defined(HAS_ODL_ASSET)
    AssetTypeBinding_t type =
    {
        .name = "On Demand Loaded Asset", // i can't think of a better way of describing this..
        .type = 'aldo',
        .headerAlignment = 8,
        .loadFunc = LoadODLAsset,
        .postLoadFunc = nullptr,
        .previewFunc = PreviewODLAsset,
        .e = { ExportODLAsset, 0, nullptr, 0ull },
    };

    REGISTER_TYPE(type);
#endif
}
