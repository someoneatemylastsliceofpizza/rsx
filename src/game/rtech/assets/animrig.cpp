#include <pch.h>
#include <game/rtech/assets/rson.h>
#include <game/rtech/assets/animrig.h>
#include <game/rtech/assets/animseq.h>

#include <core/mdl/stringtable.h>
#include <core/mdl/rmax.h>
#include <core/mdl/cast.h>

#include <thirdparty/imgui/imgui.h>
#include <thirdparty/imgui/misc/imgui_utility.h>

extern ExportSettings_t g_ExportSettings;

void LoadAnimRigAsset(CAssetContainer* const container, CAsset* const asset)
{
    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);
    AnimRigAsset* arigAsset = nullptr;

    switch (pakAsset->version())
    {
    case 4:
    {
        AnimRigAssetHeader_v4_t* const hdr = reinterpret_cast<AnimRigAssetHeader_v4_t*>(pakAsset->header());
        arigAsset = new AnimRigAsset(hdr, GetModelPakVersion(reinterpret_cast<const int* const>(hdr->data)));
        break;
    }
    case 5:
    case 6:
    {
        AnimRigAssetHeader_v5_t* const hdr = reinterpret_cast<AnimRigAssetHeader_v5_t*>(pakAsset->header());
        arigAsset = new AnimRigAsset(hdr, GetModelPakVersion(reinterpret_cast<const int* const>(hdr->data)));
        break;
    }
    case 7:
    {
        CPakFile* const pak = static_cast<CPakFile* const>(container);
        eMDLVersion ver = pak->header()->createdTime >= s_AnimSeqTimeStamp_V12_1 ? eMDLVersion::VERSION_19_1 : eMDLVersion::VERSION_19;

        AnimRigAssetHeader_v5_t* const hdr = reinterpret_cast<AnimRigAssetHeader_v5_t*>(pakAsset->header());

        // i HAAAAATE this tool man
        if (pak->header()->createdTime > s_AnimRigTimeStamp_V7_V19_2)
            ver = eMDLVersion::VERSION_19_2;

        arigAsset = new AnimRigAsset(hdr, ver);
        break;
    }
    default:
        return;
    }

    switch (arigAsset->studioVersion)
    {
    case eMDLVersion::VERSION_8:
    case eMDLVersion::VERSION_9:
    case eMDLVersion::VERSION_10:
    case eMDLVersion::VERSION_11:
    case eMDLVersion::VERSION_12:
    {
        ParseModelBoneData_v8(arigAsset->GetParsedData());
        ParseModelAttachmentData_v8(arigAsset->GetParsedData());
        ParseModelHitboxData_v8(arigAsset->GetParsedData());
        ParseModelAnimTypes_V8(arigAsset->GetParsedData());

        break;
    }
    case eMDLVersion::VERSION_12_1:
    case eMDLVersion::VERSION_12_2:
    case eMDLVersion::VERSION_12_3:
    case eMDLVersion::VERSION_12_4:
    case eMDLVersion::VERSION_12_5:
    case eMDLVersion::VERSION_13:
    case eMDLVersion::VERSION_13_1:
    case eMDLVersion::VERSION_14:
    case eMDLVersion::VERSION_14_1:
    case eMDLVersion::VERSION_15:
    {
        ParseModelBoneData_v12_1(arigAsset->GetParsedData());
        ParseModelAttachmentData_v8(arigAsset->GetParsedData());
        ParseModelHitboxData_v8(arigAsset->GetParsedData());
        ParseModelAnimTypes_V8(arigAsset->GetParsedData());

        break;
    }
    case eMDLVersion::VERSION_16:
    case eMDLVersion::VERSION_17:
    {
        ParseModelBoneData_v16(arigAsset->GetParsedData());
        ParseModelAttachmentData_v16(arigAsset->GetParsedData());
        ParseModelHitboxData_v16(arigAsset->GetParsedData());
        ParseModelAnimTypes_V16(arigAsset->GetParsedData());

        break;
    }
    case eMDLVersion::VERSION_18:
    {
        ParseModelBoneData_v16(arigAsset->GetParsedData());
        ParseModelAttachmentData_v16(arigAsset->GetParsedData());
        ParseModelHitboxData_v16(arigAsset->GetParsedData());
        ParseModelAnimTypes_V16(arigAsset->GetParsedData());

        break;
    }
    case eMDLVersion::VERSION_19:
    {
        ParseModelBoneData_v19(arigAsset->GetParsedData());
        ParseModelAttachmentData_v16(arigAsset->GetParsedData());
        ParseModelHitboxData_v16(arigAsset->GetParsedData());
        ParseModelAnimTypes_V16(arigAsset->GetParsedData());

        break;
    }
    case eMDLVersion::VERSION_19_1:
    case eMDLVersion::VERSION_19_2:
    {
        ParseModelBoneData_v19(arigAsset->GetParsedData());
        ParseModelAttachmentData_v16(arigAsset->GetParsedData());
        ParseModelHitboxData_v16(arigAsset->GetParsedData());
        ParseModelAnimTypes_V16(arigAsset->GetParsedData());

        break;
    }
    case eMDLVersion::VERSION_UNK:
    default:
    {
        assertm(false, "Unknown AnimRig asset version");
        break;
    }
    }

    assertm(arigAsset->name, "Rig had no name.");
    pakAsset->SetAssetName(arigAsset->name, true);
    pakAsset->setExtraData(arigAsset);
}

void PostLoadAnimRigAsset(CAssetContainer* const pak, CAsset* const asset)
{
    UNUSED(pak);

    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);

    AnimRigAsset* const arigAsset = reinterpret_cast<AnimRigAsset*>(pakAsset->extraData());

    if (!arigAsset)
        return;

    // parse sequences for children
    if (arigAsset->numAnimSeqs)
    {
        ModelParsedData_t* const parsedData = arigAsset->GetParsedData();

        parsedData->numExternalSequences = arigAsset->numAnimSeqs;
        parsedData->externalSequences = arigAsset->animSeqs;

        const uint64_t* guids = reinterpret_cast<const uint64_t*>(arigAsset->animSeqs);

        for (uint16_t seqIdx = 0; seqIdx < arigAsset->numAnimSeqs; seqIdx++)
        {
            const uint64_t guid = guids[seqIdx];

            CPakAsset* const animSeqAsset = g_assetData.FindAssetByGUID<CPakAsset>(guid);

            if (nullptr == animSeqAsset)
            {
                continue;
            }

            AnimSeqAsset* const animSeq = reinterpret_cast<AnimSeqAsset* const>(animSeqAsset->extraData());

            if (nullptr == animSeq)
            {
                continue;
            }

            animSeq->parentRig = !animSeq->parentRig ? arigAsset : animSeq->parentRig;
        }
    }

    // [rika]: this should never get hit
    if (arigAsset->GetParsedData()->NumLocalSeq() == 0)
        return;

    assertm(false, "arig had internal sequences");

    switch (arigAsset->studioVersion)
    {
    case eMDLVersion::VERSION_8:
    case eMDLVersion::VERSION_9:
    case eMDLVersion::VERSION_10:
    case eMDLVersion::VERSION_11:
    case eMDLVersion::VERSION_12:
    {
        ParseModelSequenceData_NoStall(arigAsset->GetParsedData(), reinterpret_cast<char* const>(arigAsset->data));

        break;
    }
    case eMDLVersion::VERSION_12_1:
    case eMDLVersion::VERSION_12_2:
    case eMDLVersion::VERSION_12_3:
    case eMDLVersion::VERSION_12_4:
    case eMDLVersion::VERSION_12_5:
    case eMDLVersion::VERSION_13:
    case eMDLVersion::VERSION_13_1:
    case eMDLVersion::VERSION_14:
    case eMDLVersion::VERSION_14_1:
    case eMDLVersion::VERSION_15:
    {
        ParseModelSequenceData_Stall_V8(arigAsset->GetParsedData(), reinterpret_cast<char* const>(arigAsset->data));

        break;
    }
    case eMDLVersion::VERSION_16:
    case eMDLVersion::VERSION_17:
    {
        ParseModelSequenceData_Stall_V16(arigAsset->GetParsedData(), reinterpret_cast<char* const>(arigAsset->data));

        break;
    }
    case eMDLVersion::VERSION_18:
    case eMDLVersion::VERSION_19:
    {
        ParseModelSequenceData_Stall_V18(arigAsset->GetParsedData(), reinterpret_cast<char* const>(arigAsset->data));

        break;
    }
    case eMDLVersion::VERSION_19_1:
    case eMDLVersion::VERSION_19_2:
    {
        ParseModelSequenceData_Stall_V19_1(arigAsset->GetParsedData(), reinterpret_cast<char* const>(arigAsset->data));

        break;
    }
    case eMDLVersion::VERSION_UNK:
    default:
    {
        assertm(false, "unaccounted asset version, will cause major issues!");
        break;
    }
    }
}

static bool ExportRawAnimRigAsset(CPakAsset* const asset, const AnimRigAsset* const animRigAsset, std::filesystem::path& exportPath)
{
    UNUSED(asset);

    StreamIO rigOut(exportPath.string(), eStreamIOMode::Write);
    rigOut.write(reinterpret_cast<const char*>(animRigAsset->data), animRigAsset->pStudioHdr()->length);
    rigOut.close();

    // make a manifest of this assets dependencies
    exportPath.replace_extension(".rson");

    StreamIO depOut(exportPath.string(), eStreamIOMode::Write);
    WriteRSONDependencyArray(*depOut.W(), "seqs", animRigAsset->animSeqs, animRigAsset->numAnimSeqs);
    depOut.close();

    return true;
}

void* PreviewAnimRigAsset(CAsset* const asset, const bool firstFrameForAsset)
{
    CPakAsset* const pakAsset = static_cast<CPakAsset*>(asset);
    assertm(pakAsset, "Asset should be valid.");

    AnimRigAsset* const rigAsset = pakAsset->extraData<AnimRigAsset*>();
    if (!rigAsset)
        return nullptr;

    static ModelPreviewInfo_t previewInfo;
    if (firstFrameForAsset)
    {
        previewInfo.bodygroupModelSelected.clear();
        previewInfo.selectedBodypartIndex = 0u;
        previewInfo.selectedSkinIndex = 0u;
        previewInfo.selectedLODLevel = 0u;
        previewInfo.minLODIndex = 0u;
        previewInfo.maxLODIndex = 0u;
        previewInfo.selectedModelGuid = 0ull;
        previewInfo.selectedSequenceGuid = 0ull;
        previewInfo.selectedRigGuid = asset->GetAssetGUID();
        previewInfo.activeMeshGuid = 0ull;
        previewInfo.selectedAnimationIndex = 0;
        previewInfo.previewTime = 0.0f;
        previewInfo.previewFrame = 0;
        previewInfo.previewAnimationPlaying = true;
        previewInfo.previewAnimationLoop = true;
        previewInfo.showBones = false;
        previewInfo.showAttachments = false;
        previewInfo.attachCameraToCameraAttachment = false;
    }

    struct PreviewOption_t
    {
        uint64_t guid;
        std::string label;
    };

    std::vector<PreviewOption_t> modelOptions;
    for (auto& lookup : g_assetData.v_assets)
    {
        CAsset* const candidateAsset = lookup.m_asset;
        if (!candidateAsset || candidateAsset->GetAssetContainerType() != CAsset::ContainerType::PAK)
            continue;

        CPakAsset* const candidate = static_cast<CPakAsset*>(candidateAsset);
        if (candidate->GetAssetType() != '_ldm' || !candidate->hasExtraData())
            continue;

        std::vector<AssetGuid_t> dependencies;
        candidate->getDependencies(dependencies);
        const bool dependsOnRig = std::ranges::any_of(dependencies, [&](const AssetGuid_t& guid) { return guid.guid == asset->GetAssetGUID(); });
        if (!dependsOnRig)
            continue;

        modelOptions.push_back({ candidate->GetAssetGUID(), candidate->GetAssetName() });
    }

    auto resolveSelectedModel = [&]() -> ModelAsset*
    {
        if (!previewInfo.selectedModelGuid)
            return nullptr;

        if (CPakAsset* const modelPak = g_assetData.FindAssetByGUID<CPakAsset>(previewInfo.selectedModelGuid))
            return modelPak->extraData<ModelAsset*>();

        return nullptr;
    };

    ModelAsset* selectedModel = resolveSelectedModel();

    const char* modelLabel = "<none>";
    if (selectedModel && selectedModel->name)
        modelLabel = selectedModel->name;

    if (ImGui::BeginCombo("Model", modelLabel))
    {
        const bool noModelSelected = (previewInfo.selectedModelGuid == 0ull);
        if (ImGui::Selectable("<none>", noModelSelected))
        {
            previewInfo.selectedModelGuid = 0ull;
            previewInfo.bodygroupModelSelected.clear();
            previewInfo.selectedAnimationIndex = 0;
        }
        if (noModelSelected)
            ImGui::SetItemDefaultFocus();

        for (const PreviewOption_t& option : modelOptions)
        {
            const bool isSelected = previewInfo.selectedModelGuid == option.guid;
            if (ImGui::Selectable(option.label.c_str(), isSelected))
            {
                previewInfo.selectedModelGuid = option.guid;
                previewInfo.bodygroupModelSelected.clear();
                previewInfo.selectedAnimationIndex = 0;
                previewInfo.previewTime = 0.0f;
            }
            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    selectedModel = resolveSelectedModel();

    std::vector<PreviewOption_t> sequenceOptions;
    sequenceOptions.reserve(rigAsset->numAnimSeqs);
    for (int i = 0; i < rigAsset->numAnimSeqs; ++i)
    {
        const uint64_t guid = rigAsset->animSeqs[i].guid;
        CPakAsset* const seqPak = g_assetData.FindAssetByGUID<CPakAsset>(guid);
        if (!seqPak || !seqPak->hasExtraData())
            continue;

        sequenceOptions.push_back({ guid, seqPak->GetAssetName() });
    }

    if (previewInfo.selectedSequenceGuid != 0ull)
    {
        const auto it = std::find_if(sequenceOptions.begin(), sequenceOptions.end(), [&](const PreviewOption_t& option) { return option.guid == previewInfo.selectedSequenceGuid; });
        if (it == sequenceOptions.end())
        {
            previewInfo.selectedSequenceGuid = 0ull;
            previewInfo.selectedAnimationIndex = 0;
        }
    }

    if (previewInfo.selectedSequenceGuid == 0ull && !sequenceOptions.empty())
        previewInfo.selectedSequenceGuid = sequenceOptions.front().guid;

    AnimSeqAsset* selectedSequence = nullptr;
    if (previewInfo.selectedSequenceGuid)
    {
        if (CPakAsset* const seqPak = g_assetData.FindAssetByGUID<CPakAsset>(previewInfo.selectedSequenceGuid))
            selectedSequence = seqPak->extraData<AnimSeqAsset*>();
    }

    const char* sequenceLabel = "<none>";
    if (selectedSequence)
        sequenceLabel = selectedSequence->seqdesc.szlabel;

    if (ImGui::BeginCombo("Sequence", sequenceLabel))
    {
        for (const PreviewOption_t& option : sequenceOptions)
        {
            const bool isSelected = previewInfo.selectedSequenceGuid == option.guid;
            if (ImGui::Selectable(option.label.c_str(), isSelected))
            {
                previewInfo.selectedSequenceGuid = option.guid;
                previewInfo.selectedAnimationIndex = 0;
                previewInfo.previewTime = 0.0f;
                previewInfo.previewFrame = 0;
            }
            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ModelParsedData_t* meshParsedData = nullptr;
    uint64_t meshGuid = 0ull;
    if (selectedModel)
    {
        meshParsedData = selectedModel->GetParsedData();
        meshGuid = previewInfo.selectedModelGuid;

        if (previewInfo.bodygroupModelSelected.size() != meshParsedData->bodyParts.size())
            previewInfo.bodygroupModelSelected.assign(meshParsedData->bodyParts.size(), 0ull);

        if (!meshParsedData->lods.empty())
        {
            previewInfo.maxLODIndex = static_cast<uint8_t>(meshParsedData->lods.size()) - 1;
            previewInfo.selectedLODLevel = std::min(previewInfo.selectedLODLevel, previewInfo.maxLODIndex);
        }
    }

    const ModelSeq_t* const previewSequence = selectedSequence ? &selectedSequence->seqdesc : nullptr;
    return PreviewParsedData(&previewInfo, meshParsedData, rigAsset->GetParsedData(), previewSequence, rigAsset->name, asset->GetAssetGUID(), meshGuid, firstFrameForAsset);
}

static const char* const s_PathPrefixARIG = s_AssetTypePaths.find(AssetType_t::ARIG)->second;
bool ExportAnimRigAsset(CAsset* const asset, const int setting)
{
    CPakAsset* pakAsset = static_cast<CPakAsset*>(asset);
    const AnimRigAsset* const animRigAsset = reinterpret_cast<AnimRigAsset*>(pakAsset->extraData());

    if (!animRigAsset)
        return false;

    assertm(animRigAsset->name, "No name for anim rig.");

    // Create exported path + asset path.
    std::filesystem::path exportPath = g_ExportSettings.GetExportDirectory();
    const std::filesystem::path rigPath(animRigAsset->name);
    const std::string rigStem(rigPath.stem().string());

    // truncate paths?
    if (g_ExportSettings.exportPathsFull)
        exportPath.append(rigPath.parent_path().string());
    else
        exportPath.append(std::format("{}/{}", s_PathPrefixARIG, rigStem));

    if (!CreateDirectories(exportPath))
    {
        assertm(false, "Failed to create asset directory.");
        return false;
    }

    const ModelParsedData_t* const parsedData = &animRigAsset->parsedData;

    if (g_ExportSettings.exportRigSequences && animRigAsset->numAnimSeqs > 0)
    {
        if (!ExportAnimSeqFromAsset(exportPath, rigStem, animRigAsset->name, animRigAsset->numAnimSeqs, animRigAsset->animSeqs, animRigAsset->GetRig()))
            return false;
    }

    if (g_ExportSettings.exportRigSequences && parsedData->NumLocalSeq() > 0)
    {
        std::filesystem::path outputPath(exportPath);
        outputPath.append(std::format("anims_{}/temp", rigStem));

        if (!CreateDirectories(outputPath.parent_path()))
        {
            assertm(false, "Failed to create directory.");
            return false;
        }

        auto aseqAssetBinding = g_assetData.m_assetTypeBindings.find('qesa');
        assertm(aseqAssetBinding != g_assetData.m_assetTypeBindings.end(), "Unable to find asset type binding for \"aseq\" assets");

        for (int i = 0; i < parsedData->NumLocalSeq(); i++)
        {
            const ModelSeq_t* const seqdesc = parsedData->LocalSeq(i);

            outputPath.replace_filename(seqdesc->szlabel);

            ExportSeqDesc(aseqAssetBinding->second.e.exportSetting, seqdesc, outputPath, animRigAsset->name, animRigAsset->GetRig(), RTech::StringToGuid(seqdesc->szlabel));
        }
    }

    // rmax & cast just export the skeleton for now, perhaps in the future we could also export IK?

    exportPath.append(std::format("{}.rrig", rigStem));

    switch (setting)
    {
    case eAnimRigExportSetting::ANIMRIG_CAST:
    {
        return ExportModelCast(parsedData, exportPath, asset->GetAssetGUID());
    }
    case eAnimRigExportSetting::ANIMRIG_RMAX:
    {
        return ExportModelRMAX(parsedData, exportPath);
    }
    case eAnimRigExportSetting::ANIMRIG_RRIG:
    {
        return ExportRawAnimRigAsset(pakAsset, animRigAsset, exportPath);
    }
    case eAnimRigExportSetting::ANIMRIG_SMD:
    {
        return ExportModelSMD(parsedData, exportPath) && ExportModelQC(parsedData, exportPath, setting, 54);
    }
    default:
    {
        assertm(false, "Export setting is not handled.");
        return false;
    }
    }

    unreachable();
}

void InitAnimRigAssetType()
{
    AssetTypeBinding_t type =
    {
        .name = "Animation Rig",
        .type = 'gira',
        .headerAlignment = 8,
        .loadFunc = LoadAnimRigAsset,
        .postLoadFunc = PostLoadAnimRigAsset,
        .previewFunc = PreviewAnimRigAsset,
        .e = { ExportAnimRigAsset, 0, s_AnimRigExportSettingNames, ARRSIZE(s_AnimRigExportSettingNames) },
    };

    REGISTER_TYPE(type);
}
