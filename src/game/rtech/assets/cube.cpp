#include "pch.h"
#include <game/rtech/assets/cube.h>
#include <imgui.h>

extern RSXSettings_t g_rsxSettings;

void LoadCubeAsset(CAssetContainer* const pak, CAsset* const asset)
{
    UNUSED(pak);
    UNUSED(asset);

    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);
    //const CubeAssetHeader_t* const header = reinterpret_cast<const CubeAssetHeader_t* const>(pakAsset->header());
    //auto internal = pakAsset->data();

    const std::string assetName = std::format("cubemap/{:X}.rpak", pakAsset->GetAssetGUID());
    pakAsset->SetAssetName(assetName);
}

void* PreviewCubeAsset(CAsset* const asset, const bool firstFrameForAsset)
{
    UNUSED(firstFrameForAsset);
    UNUSED(asset);
    return nullptr;
}

void InitCubeAssetType()
{
    static const char* settings[] = { "TXT" };
    AssetTypeBinding_t type =
    {
        .name = "Cube",
        .type = 'ebuc',
        .headerAlignment = 8,
        .loadFunc = LoadCubeAsset,
        .postLoadFunc = nullptr,
        .previewFunc = PreviewCubeAsset,
        .e = { nullptr, 0, settings, 0 },
    };

    REGISTER_TYPE(type);
}