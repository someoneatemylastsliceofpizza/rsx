#include <pch.h>
#include <game/rtech/assets/localization.h>
#include <game/rtech/cpakfile.h>

extern RSXSettings_t g_rsxSettings;

void LoadLocalizationAsset(CAssetContainer* pak, CAsset* asset)
{
    UNUSED(pak);
    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);

    LocalizationHeader_t* hdr = reinterpret_cast<LocalizationHeader_t*>(pakAsset->header());
    LocalizationAsset* loclAsset = new LocalizationAsset(hdr);

    pakAsset->SetAssetName("localization/" + loclAsset->getName() + ".locl");
    pakAsset->setExtraData(loclAsset);
}

void PostLoadLocalizationAsset(CAssetContainer* pak, CAsset* asset)
{
    UNUSED(pak);
    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);

    LocalizationAsset* loclAsset = pakAsset->extraData<LocalizationAsset*>();

    // todo: reverse actual locl usage so we can speed this up a bit
    for (size_t i = 0; i < loclAsset->numEntries; ++i)
    {
        const LocalizationEntry_t* const entry = &loclAsset->entries[i];

        if (entry->hash == 0)
            continue;

        const std::wstring wideString = &loclAsset->strings[entry->stringStartIndex];

        std::string multiByteString;
        multiByteString.resize(wideString.length() * 4);

        // windows utf-16 support sucks so convert to multibyte utf8
        int numBytes = WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), static_cast<int>(wideString.length()), &multiByteString[0], static_cast<int>(multiByteString.length()), (LPCCH)NULL, NULL);

        multiByteString.resize(numBytes);

        loclAsset->entryMap.insert({ entry->hash, multiByteString });
    }
}

static const char* const s_PathPrefixLOCL = s_AssetTypePaths.find(AssetType_t::LOCL)->second;
bool ExportLocalizationAsset(CAsset* const asset, const int setting)
{
    UNUSED(setting);
    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);

    // Create exported path + asset path.
    std::filesystem::path exportPath = g_rsxSettings.GetExportDirectory();
    const std::filesystem::path localizationPath(asset->GetAssetName());

    // truncate paths?
    if (g_rsxSettings.exportPathsFull)
        exportPath.append(localizationPath.parent_path().string());
    else
        exportPath.append(s_PathPrefixLOCL);

    if (!CreateDirectories(exportPath))
    {
        assertm(false, "Failed to create asset type directory.");
        return false;
    }

    const LocalizationAsset* const loclAsset = pakAsset->extraData<LocalizationAsset*>();

    exportPath.append(loclAsset->fileName);
    exportPath.replace_extension(".locl");

    std::ofstream ofs(exportPath, std::ios::out | std::ios::binary);

    ofs << "\"" << loclAsset->fileName << "\"\n{\n";

    for (auto& it : loclAsset->entryMap)
    {
        ofs << "\t\"" << std::hex << it.first << "\" \"" << EscapeString(it.second).c_str() << "\"\n"; // use the .c_str() function here now so it gets null terminated properly (if there's extra data or it's cut off, there will no longer be a [NUL] character in the file.
    }

    ofs << "}";

    ofs.close();

    return true;
}

void InitLoclAssetType()
{
    AssetTypeBinding_t type =
    {
        .name = "Localization",
        .type = 'lcol',
        .headerAlignment = 8,
        .loadFunc = LoadLocalizationAsset,
        .postLoadFunc = PostLoadLocalizationAsset,
        .previewFunc = nullptr,
        .e = { ExportLocalizationAsset, 0, nullptr, 0ull },
    };

    REGISTER_TYPE(type);
}