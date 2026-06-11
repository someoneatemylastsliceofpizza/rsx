#include <pch.h>
#include <core/mdl/modeldata.h>
#include <sstream>
#include <game/asset.h>
#include <game/rtech/assets/animseq.h>

#include <core/mdl/rmax.h>
#include <core/mdl/cast.h>
#include <core/mdl/smd.h>

//#include <core/render/dx.h>
//#include <thirdparty/imgui/imgui.h>
#include <thirdparty/imgui/misc/imgui_utility.h>
#include <core/render/preview/preview.h>

extern CDXParentHandler* g_dxHandler;
extern CBufferManager g_BufferManager;
extern ExportSettings_t g_ExportSettings;
extern CPreviewDrawData g_currentPreviewDrawData;

//
// PARSEDDATA
//
#define VERT_DATA(t, d, o) reinterpret_cast<const t* const>(d + o)
#define MAP_BONE(b) bigBones ? reinterpret_cast<const uint16_t* const>(boneMap)[b] : (uint16_t)reinterpret_cast<const uint8_t* const>(boneMap)[b];
bool Vertex_t::ParseVertexFromVG(Vertex_t* const vert, VertexWeight_t* const weights, Vector2D* const texcoords, ModelMeshData_t* const mesh, const char* const rawVertexData, const void* const boneMap, const vvw::mstudioboneweightextra_t* const weightExtra, bool bigBones, int& weightIdx)
{
	int offset = 0;

	// [rika]: older hwdata models used bit flags, but starting in season 11.1 (rmdl 13.1) it's treated more like an enum
	/*if (mesh->rawVertexLayoutFlags & VERT_POSITION_UNPACKED)
	{
		vert->position = *VERT_DATA(Vector, rawVertexData, offset);
		offset += sizeof(Vector);
	}

	if (mesh->rawVertexLayoutFlags & VERT_POSITION_PACKED)
	{
		vert->position = VERT_DATA(Vector64, rawVertexData, offset)->Unpack();
		offset += sizeof(Vector64);
	}*/

	const vg::eVertPositionType posType = static_cast<vg::eVertPositionType>(mesh->rawVertexLayoutFlags & 3);
	switch (posType)
	{
	case vg::eVertPositionType::VG_POS_NONE:
	{
		break;
	}
	case vg::eVertPositionType::VG_POS_UNPACKED:
	{
		vert->position = *VERT_DATA(Vector, rawVertexData, offset);
		offset += sizeof(Vector);

		break;
	}
	case vg::eVertPositionType::VG_POS_PACKED64:
	{
		vert->position = VERT_DATA(Vector64, rawVertexData, offset)->Unpack();
		offset += sizeof(Vector64);

		break;
	}
	case vg::eVertPositionType::VG_POS_PACKED48:
	{
		// [rika]: not sure the format on this one, currently only used on switch and I can't be asked to find tools to decompile a shader at this time
		vert->position = Vector(0.0f);
		offset += 0x6;

		break;
	}
	}

	assertm(nullptr != weights, "weight pointer should be valid");
	vert->weightIndex = weightIdx;

	// we have weight data
	// note: if for some reason 'VERT_BLENDWEIGHTS_UNPACKED' is encountered, weights would not be processed.
	assertm(!(mesh->rawVertexLayoutFlags & VERT_BLENDWEIGHTS_UNPACKED), "mesh had unpacked weights!");
	if (mesh->rawVertexLayoutFlags & (VERT_BLENDINDICES | VERT_BLENDWEIGHTS_PACKED))
	{
		const vg::BlendWeightsPacked_s* const blendWeights = VERT_DATA(vg::BlendWeightsPacked_s, rawVertexData, offset);
		const vg::BlendWeightIndices_s* const blendIndices = VERT_DATA(vg::BlendWeightIndices_s, rawVertexData, offset + 4);

		offset += 8;

		// Copy blend data into the vert struct
		memcpy_s(&vert->blendData, sizeof(vert->blendData), blendWeights, sizeof(vert->blendData));

		uint8_t curIdx = 0; // current weight
		uint16_t remaining = 32767; // 'weight' remaining to assign to the last bone

		// model has more than 3 weights per vertex
		if (nullptr != weightExtra)
		{
			assertm(blendIndices->boneCount < 16, "model had more than 16 bones on complex weights");

			// first weight, we will always have this
			const int16_t firstBoneIndex = bigBones ? static_cast<uint16_t>(blendIndices->Packed()->firstBone) : blendIndices->bone[0];
			weights[curIdx].bone = MAP_BONE(firstBoneIndex);
			weights[curIdx].weight = blendWeights->Weight(0);
			remaining -= blendWeights->weight[0];

			curIdx++;

			// only hit if we have over 2 bones/weights
			for (uint8_t i = curIdx; i < blendIndices->boneCount; i++)
			{
				auto extraWeight = weightExtra[blendWeights->ExtraWeightsStartIndex() + (curIdx - 1)];

				weights[curIdx].bone = MAP_BONE(extraWeight.bone);
				weights[curIdx].weight = extraWeight.Weight();

				remaining -= extraWeight.weight;

				curIdx++;
			}

			// only hit if we have over 1 bone/weight
			if (blendIndices->boneCount > 0)
			{
				// im just using bigBones as a flag for >=v19.2 lmao
				const int16_t finalBoneIndex = bigBones ? static_cast<uint16_t>(blendIndices->Packed()->lastBone) : blendIndices->bone[1];
				weights[curIdx].bone = MAP_BONE(finalBoneIndex);
				weights[curIdx].weight = UNPACKWEIGHT(remaining);

				curIdx++;
			}
		}
		else
		{
			assertm(blendIndices->boneCount < 3, "model had more than 3 bones on simple weights");

			// There seems to be a condition here on v19.2, where if the model uses simple weights and this vert has less than two "extra" bones (i.e. boneCount < 2),
			// the blend indices use the same packed system as the complex weights. If the model uses simple weights and has TWO extra bones, they revert to the old system?
			//const bool singleExtraBigBone = bigBones && blendIndices->boneCount == 1;
			for (uint8_t i = 0; i < blendIndices->boneCount; i++)
			{
				weights[curIdx].bone = MAP_BONE(blendIndices->bone[curIdx]);
				weights[curIdx].weight = blendWeights->Weight(curIdx);

				remaining -= blendWeights->weight[curIdx];

				curIdx++;
			}

			weights[curIdx].bone = MAP_BONE(blendIndices->bone[curIdx]);
			weights[curIdx].weight = UNPACKWEIGHT(remaining);

			curIdx++;
		}

		vert->weightCount = curIdx;
		assert(static_cast<uint8_t>(vert->weightCount) == (blendIndices->boneCount + 1)); // numbones is really 'extra' bones on top of the base weight, verify the count is correct

		weightIdx += curIdx;
	}

	// our mesh does not have weight data, use a set of default weights. 
	// [rika]: this can only happen when a model has one bone
	else
	{
		vert->weightCount = 1;
		weights[0].bone = 0;
		weights[0].weight = 1.0f;

		weightIdx++;
	}

	mesh->weightsPerVert = static_cast<uint16_t>(vert->weightCount) > mesh->weightsPerVert ? static_cast<uint16_t>(vert->weightCount) : mesh->weightsPerVert;

	vert->normalPacked = *VERT_DATA(Normal32, rawVertexData, offset);
	offset += sizeof(Normal32);

	if (mesh->rawVertexLayoutFlags & VERT_COLOR)
	{
		// Vertex Colour
		vert->color = *VERT_DATA(Color32, rawVertexData, offset);
		offset += sizeof(Color32);
	}
	else // no vert colour, write default
	{
		vert->color = Color32(255, 255);
	}

	if (mesh->rawVertexLayoutFlags & VERT_TEXCOORD0)
	{
		vert->texcoord = *VERT_DATA(Vector2D, rawVertexData, offset);
		offset += sizeof(Vector2D);
	}

	for (int localIdx = 1, countIdx = 1; countIdx < mesh->texcoordCount; localIdx++)
	{
		assertm(nullptr != texcoords, "texcoord pointer should be valid");

		if (!VERT_TEXCOORDn(localIdx))
			continue;

		texcoords[countIdx - 1] = *VERT_DATA(Vector2D, rawVertexData, offset);
		offset += sizeof(Vector2D);

		countIdx++;
	}

	assertm(offset == mesh->vertCacheSize, "parsed data size differed from vertexCacheSize");

	return true;
}
#undef VERT_DATA

// Generic (basic data shared between them)
void Vertex_t::ParseVertexFromVTX(Vertex_t* const vert, Vector2D* const texcoords, ModelMeshData_t* const mesh, const vvd::mstudiovertex_t* const pVerts, const Vector4D* const pTangs, const Color32* const pColors, const Vector2D* const pUVs, const int origId)
{
	vert->position = pVerts[origId].m_vecPosition;

	// not normal
	vert->normalPacked.PackNormal(pVerts[origId].m_vecNormal, pTangs[origId]);

	if (pColors)
		vert->color = pColors[origId];
	else // no vert colour, write default
		vert->color = Color32(255, 255);

	vert->texcoord = pVerts[origId].m_vecTexCoord;

	for (int localIdx = 1, countIdx = 1; countIdx < mesh->texcoordCount; localIdx++)
	{
		assertm(nullptr != texcoords, "texcoord pointer should be valid");

		if (!VERT_TEXCOORDn(localIdx))
			continue;

		texcoords[countIdx - 1] = pUVs[origId]; // [rika]: add proper support for uv3 (though I doubt we'll ever get files for it, I think it would be quite quirky.)

		countIdx++;
	}
}

// Basic Source
void Vertex_t::ParseVertexFromVTX(Vertex_t* const vert, VertexWeight_t* const weights, Vector2D* const texcoords, ModelMeshData_t* mesh, const OptimizedModel::Vertex_t* const pVertex, const vvd::mstudiovertex_t* const pVerts, const Vector4D* const pTangs, const Color32* const pColors, const Vector2D* const pUVs,
	int& weightIdx, const bool isHwSkinned, const OptimizedModel::BoneStateChangeHeader_t* const pBoneStates)
{
	const int origId = pVertex->origMeshVertID;

	ParseVertexFromVTX(vert, texcoords, mesh, pVerts, pTangs, pColors, pUVs, origId);

	const vvd::mstudiovertex_t& oldVert = pVerts[origId];

	assertm(nullptr != weights, "weight pointer should be valid");
	vert->weightIndex = weightIdx;
	vert->weightCount = oldVert.m_BoneWeights.numbones > 0 ? oldVert.m_BoneWeights.numbones : 1;

	if (oldVert.m_BoneWeights.numbones <= 0)
	{
		weights[0].weight = 1.0f;
		weights[0].bone = isHwSkinned && pBoneStates
			? static_cast<int16_t>(pBoneStates[pVertex->boneID[0]].newBoneID)
			: pVertex->boneID[0];

		weightIdx += 1;
		mesh->weightsPerVert = static_cast<uint16_t>(vert->weightCount) > mesh->weightsPerVert ? static_cast<uint16_t>(vert->weightCount) : mesh->weightsPerVert;
		return;
	}

	for (int i = 0; i < oldVert.m_BoneWeights.numbones; i++)
	{
		weights[i].weight = oldVert.m_BoneWeights.weight[pVertex->boneWeightIndex[i]];

		// static props can be hardware skinned, but have no bonestates (one bone). this make it skip this, however it's a non issue as the following statement will work fine for this (there is only one bone at idx 0)
		if (isHwSkinned && pBoneStates)
		{
			weights[i].bone = static_cast<int16_t>(pBoneStates[pVertex->boneID[i]].newBoneID);
			continue;
		}

		weights[i].bone = pVertex->boneID[i];
	}

	weightIdx += oldVert.m_BoneWeights.numbones;

	mesh->weightsPerVert = static_cast<uint16_t>(vert->weightCount) > mesh->weightsPerVert ? static_cast<uint16_t>(vert->weightCount) : mesh->weightsPerVert;
}

// Apex Legends
void Vertex_t::ParseVertexFromVTX(Vertex_t* const vert, VertexWeight_t* const weights, Vector2D* const texcoords, ModelMeshData_t* mesh, const OptimizedModel::Vertex_t* const pVertex, const vvd::mstudiovertex_t* const pVerts, const Vector4D* const pTangs, const Color32* const pColors, const Vector2D* const pUVs,
	const vvw::vertexBoneWeightsExtraFileHeader_t* const pVVW, int& weightIdx)
{
	const int origId = pVertex->origMeshVertID;

	ParseVertexFromVTX(vert, texcoords, mesh, pVerts, pTangs, pColors, pUVs, origId);

	const vvd::mstudiovertex_t& oldVert = pVerts[origId];

	assertm(nullptr != weights, "weight pointer should be valid");
	vert->weightIndex = weightIdx;
	vert->weightCount = oldVert.m_BoneWeights.numbones > 0 ? oldVert.m_BoneWeights.numbones : 1;

	if (oldVert.m_BoneWeights.numbones <= 0)
	{
		weights[0].bone = oldVert.m_BoneWeights.bone[0];
		weights[0].weight = 1.0f;

		weightIdx += 1;
		mesh->weightsPerVert = static_cast<uint16_t>(vert->weightCount) > mesh->weightsPerVert ? static_cast<uint16_t>(vert->weightCount) : mesh->weightsPerVert;
		return;
	}

	if (nullptr != pVVW)
	{
		const vvw::mstudioboneweightextra_t* const pExtraWeights = pVVW->GetWeightData(oldVert.m_BoneWeights.weightextra.extraweightindex);

		for (int i = 0; i < oldVert.m_BoneWeights.numbones; i++)
		{
			if (i >= 3)
			{
				weights[i].bone = pExtraWeights[i - 3].bone;
				weights[i].weight = pExtraWeights[i - 3].Weight();

				continue;
			}

			weights[i].bone = oldVert.m_BoneWeights.bone[i];
			weights[i].weight = oldVert.m_BoneWeights.weightextra.Weight(i);
		}
	}
	else
	{
		for (int i = 0; i < oldVert.m_BoneWeights.numbones; i++)
		{
			weights[i].bone = oldVert.m_BoneWeights.bone[i];
			weights[i].weight = oldVert.m_BoneWeights.weight[i];
		}
	}

	weightIdx += oldVert.m_BoneWeights.numbones;

	mesh->weightsPerVert = static_cast<uint16_t>(vert->weightCount) > mesh->weightsPerVert ? static_cast<uint16_t>(vert->weightCount) : mesh->weightsPerVert;
}

void ModelMeshData_t::ParseTexcoords()
{
	if (rawVertexLayoutFlags & VERT_TEXCOORD0)
	{
		int texCoordIdx = 0;
		int texCoordShift = 24;

		uint64_t inputFlagsShifted = rawVertexLayoutFlags >> texCoordShift;
		do
		{
			inputFlagsShifted = rawVertexLayoutFlags >> texCoordShift;

			int8_t texCoordFormat = inputFlagsShifted & VERT_TEXCOORD_MASK;

			assertm(texCoordFormat == 2 || texCoordFormat == 0, "invalid texcoord format");

			if (texCoordFormat != 0)
			{
				texcoordCount++;
				texcoodIndices |= (1 << texCoordIdx);
			}

			texCoordShift += VERT_TEXCOORD_BITS;
			texCoordIdx++;
		} while (inputFlagsShifted >= (1 << VERT_TEXCOORD_BITS)); // while the flag value is large enough that there is more than just one 
	}
}

void ModelMeshData_t::ParseMaterial(ModelParsedData_t* const parsed, const int material)
{
	// [rika]: handling mesh's material here since we've already looped through everything
	assertm(material < parsed->materials.size() && material >= 0, "invalid mesh material index");
	materialId = material;
	materialAsset = parsed->materials.at(material).asset;
}

// bones
void ParseModelBoneData_v8(ModelParsedData_t* const parsedData)
{
	const studiohdr_generic_t* const pStudioHdr = parsedData->pStudioHdr();

	const r5::mstudiobone_v8_t* const bones = reinterpret_cast<const r5::mstudiobone_v8_t* const>(pStudioHdr->pBones());
	parsedData->bones.resize(pStudioHdr->boneCount);

	for (int i = 0; i < pStudioHdr->boneCount; i++)
	{
		parsedData->bones.at(i) = ModelBone_t(&bones[i]);
	}
}

void ParseModelBoneData_v12_1(ModelParsedData_t* const parsedData)
{
	const studiohdr_generic_t* const pStudioHdr = parsedData->pStudioHdr();

	const r5::mstudiobone_v12_1_t* const bones = reinterpret_cast<const r5::mstudiobone_v12_1_t* const>(pStudioHdr->pBones());
	parsedData->bones.resize(pStudioHdr->boneCount);

	for (int i = 0; i < pStudioHdr->boneCount; i++)
	{
		parsedData->bones.at(i) = ModelBone_t(&bones[i]);
	}
}

void ParseModelBoneData_v16(ModelParsedData_t* const parsedData)
{
	const studiohdr_generic_t* const pStudioHdr = parsedData->pStudioHdr();

	const r5::mstudiobonehdr_v16_t* const bonehdrs = reinterpret_cast<const r5::mstudiobonehdr_v16_t* const>(pStudioHdr->pBones());
	const r5::mstudiobonedata_v16_t* const bonedata = reinterpret_cast<const r5::mstudiobonedata_v16_t* const>(pStudioHdr->pBoneData());

	parsedData->bones.resize(pStudioHdr->boneCount);

	for (int i = 0; i < pStudioHdr->boneCount; i++)
		parsedData->bones.at(i) = ModelBone_t(&bonehdrs[i], &bonedata[i]);
}

void ParseModelBoneData_v19(ModelParsedData_t* const parsedData)
{
	const studiohdr_generic_t* const pStudioHdr = parsedData->pStudioHdr();

	const r5::mstudiobonehdr_v16_t* const bonehdrs = reinterpret_cast<const r5::mstudiobonehdr_v16_t* const>(pStudioHdr->pBones());
	const r5::mstudiobonedata_v19_t* const bonedata = reinterpret_cast<const r5::mstudiobonedata_v19_t* const>(pStudioHdr->pBoneData());
	const r5::mstudiolinearbone_v19_t* const linearbone = reinterpret_cast<const r5::mstudiolinearbone_v19_t* const>(pStudioHdr->pLinearBone());

	parsedData->bones.resize(pStudioHdr->boneCount);

	for (int i = 0; i < pStudioHdr->boneCount; i++)
		parsedData->bones.at(i) = ModelBone_t(&bonehdrs[i], &bonedata[i], linearbone, i);
}

void ParseModelAttachmentData_v8(ModelParsedData_t* const parsedData)
{
	const studiohdr_generic_t* const pStudioHdr = parsedData->pStudioHdr();
	const r5::mstudioattachment_v8_t* const pAttachments = reinterpret_cast<const r5::mstudioattachment_v8_t* const>(pStudioHdr->baseptr + pStudioHdr->localAttachmentOffset);

	parsedData->attachments.reserve(pStudioHdr->localAttachmentCount);

	for (int i = 0; i < pStudioHdr->localAttachmentCount; i++)
	{
		parsedData->attachments.emplace_back(pAttachments + i);
	}
}

void ParseModelAttachmentData_v16(ModelParsedData_t* const parsedData)
{
	const studiohdr_generic_t* const pStudioHdr = parsedData->pStudioHdr();
	const r5::mstudioattachment_v16_t* const pAttachments = reinterpret_cast<const r5::mstudioattachment_v16_t* const>(pStudioHdr->baseptr + pStudioHdr->localAttachmentOffset);

	parsedData->attachments.reserve(pStudioHdr->localAttachmentCount);

	for (int i = 0; i < pStudioHdr->localAttachmentCount; i++)
	{
		parsedData->attachments.emplace_back(pAttachments + i);
	}
}

void ParseModelHitboxData_v8(ModelParsedData_t* const parsedData)
{
	const studiohdr_generic_t* const pStudioHdr = parsedData->pStudioHdr();
	const mstudiohitboxset_t* const pHitboxSets = reinterpret_cast<const mstudiohitboxset_t* const>(pStudioHdr->baseptr + pStudioHdr->hitboxSetOffset);

	parsedData->hitboxsets.reserve(pStudioHdr->hitboxSetCount);

	for (int i = 0; i < pStudioHdr->hitboxSetCount; i++)
	{
		parsedData->hitboxsets.emplace_back(pHitboxSets + i, pHitboxSets->pHitbox<r5::mstudiobbox_v8_t>(0));
	}
}

void ParseModelHitboxData_v16(ModelParsedData_t* const parsedData)
{
	const studiohdr_generic_t* const pStudioHdr = parsedData->pStudioHdr();
	const r5::mstudiohitboxset_v16_t* const pHitboxSets = reinterpret_cast<const r5::mstudiohitboxset_v16_t* const>(pStudioHdr->baseptr + pStudioHdr->hitboxSetOffset);

	parsedData->hitboxsets.reserve(pStudioHdr->hitboxSetCount);

	for (int i = 0; i < pStudioHdr->hitboxSetCount; i++)
	{
		parsedData->hitboxsets.emplace_back(pHitboxSets + i);
	}
}

void CreateBuffersForModelHitboxes(ModelParsedData_t* const parsedData, CDXDrawData* const drawData)
{
	CShader* vertexShader = g_dxHandler->GetShaderManager()->LoadShaderFromString("shaders/model_vs", s_PreviewVertexShader, eShaderType::Vertex);;
	CShader* pixelShader = g_dxHandler->GetShaderManager()->LoadShaderFromString("shaders/model_ps", s_PreviewPixelShader, eShaderType::Pixel);

	for (auto& hitboxSet : parsedData->hitboxsets)
	{
		for (int i = 0; i < hitboxSet.numHitboxes; ++i)
		{
			const ModelHitbox_t& h = hitboxSet.hitboxes[i];

			Vector bonePos = parsedData->bones[h.bone].pos;
			Vector bbmin = (*h.bbmin) + bonePos;
			Vector bbmax = (*h.bbmax) + bonePos;

			// -8: x y z
			// -7: X y z
			// -6: x Y z
			// -5: x y Z
			// -4: X Y z
			// -3: X y Z
			// -2: x Y Z
			// -1: X Y Z

			const std::vector<Vertex_t> vertices = {
				{bbmin.x, bbmin.y, bbmin.z},
				{bbmax.x, bbmin.y, bbmin.z},
				{bbmin.x, bbmax.y, bbmin.z},
				{bbmin.x, bbmin.y, bbmax.z},
				{bbmax.x, bbmax.y, bbmin.z},
				{bbmax.x, bbmin.y, bbmax.z},
				{bbmin.x, bbmax.y, bbmax.z},
				{bbmax.x, bbmax.y, bbmax.z},
			};

			const std::vector<uint16_t> indices = {
				2, 4, 0,
				0, 4, 1,
				5, 3, 1,
				1, 3, 0,
				4, 7, 1,
				1, 7, 5,
				6, 7, 2,
				2, 7, 4,
				7, 6, 5,
				5, 6, 3,
				6, 2, 3,
				3, 2, 0
			};

			DXMeshDrawData_t& meshDrawData = drawData->meshBuffers.emplace_back();

			meshDrawData.visible = false;
			meshDrawData.doFrustumCulling = false;
			meshDrawData.wireframe = true;
			meshDrawData.indexFormat = DXGI_FORMAT_R16_UINT;
			meshDrawData.vertexShader = vertexShader->Get<ID3D11VertexShader>();
			meshDrawData.pixelShader = pixelShader->Get<ID3D11PixelShader>();
			meshDrawData.inputLayout = vertexShader->GetInputLayout();
			meshDrawData.hasGameShaders = false;

			if (!meshDrawData.vertexBuffer)
			{
				constexpr UINT vertStride = sizeof(Vertex_t);

				D3D11_BUFFER_DESC desc = {};

				desc.Usage = D3D11_USAGE_DYNAMIC;
				desc.ByteWidth = static_cast<UINT>(vertStride * vertices.size());
				desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
				desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
				desc.MiscFlags = 0;

				D3D11_SUBRESOURCE_DATA srd{ vertices.data() };

				if (FAILED(g_dxHandler->GetDevice()->CreateBuffer(&desc, &srd, &meshDrawData.vertexBuffer)))
					return;

				meshDrawData.vertexStride = vertStride;
			}

			if (!meshDrawData.indexBuffer)
			{
				D3D11_BUFFER_DESC desc = {};

				desc.Usage = D3D11_USAGE_DYNAMIC;
				desc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint16_t));
				desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
				desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
				desc.MiscFlags = 0;

				D3D11_SUBRESOURCE_DATA srd = { indices.data() };
				if (FAILED(g_dxHandler->GetDevice()->CreateBuffer(&desc, &srd, &meshDrawData.indexBuffer)))
					return;

				meshDrawData.numIndices = indices.size();
			}
		}
	}
}

void CreateBuffersForModelDrawData(ModelParsedData_t* const parsedData, CDXDrawData* const drawData, const uint64_t lod)
{
	// [rika]: eventually parse through models
	for (size_t i = 0; i < parsedData->lods.at(lod).meshes.size(); ++i)
	{
		const ModelMeshData_t& mesh = parsedData->lods.at(lod).meshes.at(i);
		DXMeshDrawData_t* const meshDrawData = &drawData->meshBuffers[i];

		meshDrawData->visible = true;
		meshDrawData->doFrustumCulling = false;
		meshDrawData->wireframe = false;

		if (mesh.materialAsset)
		{
			MaterialAsset* matl = mesh.GetMaterialAsset();
			meshDrawData->uberStaticBuf = matl->uberStaticBuffer;
			meshDrawData->uberDynamicBuf = matl->uberDynamicBuffer;
		}

		assertm(mesh.meshVertexDataIndex != invalidNoodleIdx, "mesh data hasn't been parsed ??");

		std::unique_ptr<char[]> parsedVertexDataBuf = parsedData->meshVertexData.getIdx(mesh.meshVertexDataIndex);
		const CMeshData* const parsedVertexData = reinterpret_cast<CMeshData*>(parsedVertexDataBuf.get());

		if (!meshDrawData->vertexBuffer)
		{
			constexpr UINT vertStride = sizeof(Vertex_t);

			D3D11_BUFFER_DESC desc = {};

			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.ByteWidth = vertStride * mesh.vertCount;
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			desc.MiscFlags = 0;

			const void* vertexData = parsedVertexData->GetVertices();

			D3D11_SUBRESOURCE_DATA srd{ vertexData };

			if (FAILED(g_dxHandler->GetDevice()->CreateBuffer(&desc, &srd, &meshDrawData->vertexBuffer)))
				return;

			meshDrawData->vertexStride = vertStride;
		}

		if (!meshDrawData->indexBuffer)
		{
			D3D11_BUFFER_DESC desc = {};

			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.ByteWidth = static_cast<UINT>(mesh.indexCount * sizeof(uint16_t));
			desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			desc.MiscFlags = 0;

			D3D11_SUBRESOURCE_DATA srd = { parsedVertexData->GetIndices() };
			if (FAILED(g_dxHandler->GetDevice()->CreateBuffer(&desc, &srd, &meshDrawData->indexBuffer)))
				return;

			meshDrawData->numIndices = mesh.indexCount;
		}

		if (!meshDrawData->weightsBuffer)
		{
			VertexWeight_t* const weights = parsedVertexData->GetWeights();
			const int64_t numWeights = parsedVertexData->GetWeightCount();

			VertexWeight_ForShader_t* wfs = new VertexWeight_ForShader_t[numWeights];
			for (int64_t j = 0; j < numWeights; ++j)
				wfs[j] = weights[j];

			if (CreateD3DBuffer(g_dxHandler->GetDevice(),
				&meshDrawData->weightsBuffer, static_cast<UINT>(numWeights) * sizeof(VertexWeight_ForShader_t),
				D3D11_USAGE_DYNAMIC, D3D11_BIND_SHADER_RESOURCE,
				D3D11_CPU_ACCESS_WRITE, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, sizeof(VertexWeight_ForShader_t), wfs
			))
			{
				D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
				desc.Format = DXGI_FORMAT_UNKNOWN;
				desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
				desc.Buffer.FirstElement = 0;
				desc.Buffer.NumElements = static_cast<UINT>(numWeights);

				HRESULT hr = g_dxHandler->GetDevice()->CreateShaderResourceView(meshDrawData->weightsBuffer, &desc, &meshDrawData->weightsSRV);

				UNUSED(hr);
				assert(SUCCEEDED(hr));
			}

			delete[] wfs;
		}
	}

	return;
}


//
// COMPDATA
//

// CMeshData
void CMeshData::AddIndices(const uint16_t* const indices, const size_t indiceCount)
{
	assertm(writer, "attempting to write data, but writer is not initialized.");

	indiceOffset = writer - reinterpret_cast<char*>(this);

	const size_t bufferSize = indiceCount * sizeof(uint16_t);
	assertm((writer + bufferSize) - reinterpret_cast<char*>(this) < managedBufferSize, "data exceeded buffer size!!!");
	if (indices)
		memcpy(writer, indices, bufferSize);

	writer += IALIGN16(bufferSize);
}

void CMeshData::AddVertices(const Vertex_t* const vertices, const size_t vertexCount)
{
	assertm(writer, "attempting to write data, but writer is not initialized.");

	vertexOffset = writer - reinterpret_cast<char*>(this);

	const size_t bufferSize = vertexCount * sizeof(Vertex_t);
	assertm((writer + bufferSize) - reinterpret_cast<char*>(this) < managedBufferSize, "data exceeded buffer size!!!");
	if (vertices)
		memcpy(writer, vertices, bufferSize);

	writer += IALIGN16(bufferSize);
}

void CMeshData::AddWeights(const VertexWeight_t* const weights, const size_t _weightCount)
{
	assertm(writer, "attempting to write data, but writer is not initialized.");

	weightOffset = writer - reinterpret_cast<char*>(this);

	const size_t bufferSize = _weightCount * sizeof(VertexWeight_t);
	assertm((writer + bufferSize) - reinterpret_cast<char*>(this) < managedBufferSize, "data exceeded buffer size!!!");
	if (weights)
		memcpy(writer, weights, bufferSize);

	this->weightCount = _weightCount;

	writer += IALIGN16(bufferSize);
}

void CMeshData::AddTexcoords(const Vector2D* const texcoords, const size_t texcoordCount)
{
	assertm(writer, "attempting to write data, but writer is not initialized.");

	texcoordOffset = writer - reinterpret_cast<char*>(this);

	const size_t bufferSize = texcoordCount * sizeof(Vector2D);
	assertm((writer + bufferSize) - reinterpret_cast<char*>(this) < managedBufferSize, "data exceeded buffer size!!!");
	if (texcoords)
		memcpy(writer, texcoords, bufferSize);

	writer += IALIGN16(bufferSize);
}

// CAnimData
CAnimData::CAnimData(char* const buf) : pBuffer(buf), memory(true)
{
	assertm(nullptr != pBuffer, "invalid pointer provided");

	const char* curpos = pBuffer;

	memcpy(&numBones, curpos, sizeof(int) * 2);
	curpos += sizeof(int) * 2;

	pOffsets = reinterpret_cast<const size_t* const>(curpos);
	curpos += IALIGN16(sizeof(size_t) * numBones);

	pFlags = reinterpret_cast<const uint8_t*>(curpos);
	curpos += IALIGN16(sizeof(uint8_t) * numBones);
};

// access memory data
const Vector* const CAnimData::GetBonePosForFrame(const int bone, const int frame) const
{
	const uint8_t boneFlags = GetFlag(bone);
	assertm(boneFlags & CAnimDataBone::ANIMDATA_POS, "bone did not have position");

	const Vector* const tmp = reinterpret_cast<const Vector* const>(pBuffer + pOffsets[bone]);

	return tmp + frame;
}

const Quaternion* const CAnimData::GetBoneQuatForFrame(const int bone, const int frame) const
{
	const uint8_t boneFlags = GetFlag(bone);
	assertm(boneFlags & CAnimDataBone::ANIMDATA_ROT, "bone did not have rotation");

	const Quaternion* const tmp = reinterpret_cast<const Quaternion* const>(pBuffer + pOffsets[bone] + (s_AnimDataBoneSizeLUT[boneFlags & CAnimDataBone::ANIMDATA_POS] * numFrames));

	return tmp + frame;
}

const Vector* const CAnimData::GetBoneScaleForFrame(const int bone, const int frame) const
{
	const uint8_t boneFlags = GetFlag(bone);
	assertm(boneFlags & CAnimDataBone::ANIMDATA_SCL, "bone did not have scale");

	const Vector* const tmp = reinterpret_cast<const Vector* const>(pBuffer + pOffsets[bone] + (s_AnimDataBoneSizeLUT[boneFlags & (CAnimDataBone::ANIMDATA_POS | CAnimDataBone::ANIMDATA_ROT)] * numFrames));

	return tmp + frame;
}

const size_t CAnimData::ToMemory(char* const buf)
{
	char* curpos = buf;

	// dumb
	memcpy(curpos, &numBones, sizeof(int) * 2);
	curpos += sizeof(int) * 2;

	size_t* offsets = reinterpret_cast<size_t*>(curpos);
	curpos += IALIGN16(sizeof(size_t) * numBones);

	uint8_t* flags = reinterpret_cast<uint8_t*>(curpos);
	curpos += IALIGN16(sizeof(uint8_t) * numBones);

	for (size_t i = 0; i < numBones; i++)
	{
		const CAnimDataBone& bone = bones.at(i);

		offsets[i] = static_cast<size_t>(curpos - buf);

		flags[i] = bone.GetFlags();

		if (flags[i] & CAnimDataBone::ANIMDATA_POS)
		{
			memcpy(curpos, bone.GetPosPtr(), sizeof(Vector) * numFrames);
			curpos += sizeof(Vector) * numFrames;
		}

		if (flags[i] & CAnimDataBone::ANIMDATA_ROT)
		{
			memcpy(curpos, bone.GetRotPtr(), sizeof(Quaternion) * numFrames);
			curpos += sizeof(Quaternion) * numFrames;
		}

		if (flags[i] & CAnimDataBone::ANIMDATA_SCL)
		{
			memcpy(curpos, bone.GetSclPtr(), sizeof(Vector) * numFrames);
			curpos += sizeof(Vector) * numFrames;
		}
	}

	const size_t size = static_cast<size_t>(curpos - buf);
	assertm(size < CBufferManager::MaxBufferSize(), "animation data too large");

	return size;
};


//
// EXPORT SETTINGS
//

struct ModelMaterialExport_t
{
	ModelMaterialExport_t(MaterialAsset* const material, const int materialId) : asset(material), id(materialId) {}

	std::unordered_map<uint32_t, MaterialTextureExportInfo_s> textures;
	MaterialAsset* asset;
	int id;
};

// export materials from parsed data
void HandleModelMaterials(const ModelParsedData_t* const parsedData, std::unordered_map<int, ModelMaterialExport_t>& materials, const std::filesystem::path& exportPath)
{
	// [rika]: this will decide if we want textures/materials local to the model, or to use full paths in the future.
	const bool useFullPaths = false; // temp

	// [rika]: make sure materials are empty
	materials.clear();
	materials.reserve(parsedData->materials.size());

	// [rika]: pick a skin !
	if (g_ExportSettings.exportModelSkin && g_ExportSettings.previewedSkinIndex >= static_cast<int>(parsedData->skins.size()))
	{
		assertm(false, "skin index out of range");
		g_ExportSettings.previewedSkinIndex = 0;
	}

	const int skin = g_ExportSettings.exportModelSkin ? g_ExportSettings.previewedSkinIndex : 0;

	const ModelSkinData_t* const skinData = &parsedData->skins.at(skin);

	// [rika]: we don't need to cycle through all the LODs here, any used mesh should be in the top LOD and cannot change per LOD
	// this gets the data we need to export materials, and set them properly in meshes later
	// keep track of the materials that are actually used by meshes, so we don't export unneeded ones (speeds up export)
	for (const ModelMeshData_t& mesh : parsedData->lods.front().meshes)
	{
		// [rika]: no vertices, this mesh will not be exported.
		if (!mesh.vertCount)
			continue;

		const int baseId = mesh.materialId;
		const int skinId = static_cast<int>(skinData->indices[baseId]);

		const ModelMaterialData_t* const materialData = parsedData->pMaterial(skinId);
		MaterialAsset* asset = materialData->GetMaterialAsset();

		ModelMaterialExport_t material(asset, skinId);

		if (!asset)
		{
			materials.emplace(baseId, material);

			continue;
		}

		ParseMaterialTextureExportInfo(material.textures, material.asset, exportPath, eTextureExportName::TXTR_NAME_TEXT, useFullPaths);

		materials.emplace(baseId, material);
	}

	// [rika]: don't export material textures if it's not enabled
	if (!g_ExportSettings.exportMaterialTextures)
		return;

	// [rika]: export material textures
	std::atomic<uint32_t> remainingMaterials = 0; // we don't actually need thread safe here
	const ProgressBarEvent_t* const materialExportProgress = g_pImGuiHandler->AddProgressBarEvent("Exporting Materials..", static_cast<uint32_t>(materials.size()), &remainingMaterials, true);

	// [rika]: so we don't export textures per lod, we should exclude skins
	// todo: move this into the base function, don't export if raw
	for (const auto& it : materials)
	{
		++remainingMaterials;

		const ModelMaterialExport_t& material = it.second;

		if (!material.asset)
			continue;

		const MaterialAsset* const matlAsset = material.asset;

		// skip this material if it has no textures
		if (matlAsset->txtrAssets.size() == 0ull)
			continue;

		// enable exporting other image formats! (would if blender didn't smell)
		// [rika]: the extension also needs to be altered in model export formats if we do this!
		ExportMaterialTextures(eTextureExportSetting::PNG_HM, matlAsset, material.textures); // NOTE: LOOK INTO MAKING A FOLDER PER MATERIAL ?

	}
	g_pImGuiHandler->FinishProgressBarEvent(materialExportProgress);
}

// [rika]: todo also fix this up
// export parsed data to rmax
bool ExportModelRMAX(const ModelParsedData_t* const parsedData, std::filesystem::path& exportPath)
{
	std::string fileNameBase = exportPath.stem().string();
	const std::filesystem::path filePath(exportPath.parent_path());

	rmax::RMAXExporter rmaxFile(filePath, fileNameBase.c_str(), fileNameBase.c_str());

	// do bones
	rmaxFile.ReserveBones(parsedData->bones.size());
	for (auto& bone : parsedData->bones)
		rmaxFile.AddBone(bone.name, bone.parent, bone.pos, bone.quat, bone.scale);

	// [rika]: model is skin and bones, no meat
	if (parsedData->lods.size() == 0)
	{
		const std::string tmpName(std::format("{}.rmax", fileNameBase));
		rmaxFile.SetName(tmpName.c_str());

		rmaxFile.ToFile();

		return true;
	}

	const std::filesystem::path texturePath(std::format("{}/{}", filePath.string(), fileNameBase)); // todo, remove duplicate code
	std::unordered_map<int, ModelMaterialExport_t> materials;
	HandleModelMaterials(parsedData, materials, texturePath);

	// [rika]: now we parse lods
	for (size_t lodIdx = 0; lodIdx < parsedData->lods.size(); lodIdx++)
	{
		const ModelLODData_t& lodData = parsedData->lods.at(lodIdx);

		const std::string tmpName = std::format("{}_LOD{}.rmax", fileNameBase, lodIdx);
		rmaxFile.SetName(tmpName.c_str());

		// do materials
		rmaxFile.ReserveMaterials(parsedData->materials.size());
		for (size_t matIdx = 0; matIdx < parsedData->materials.size(); matIdx++)
		{
			const ModelMaterialData_t& materialData = parsedData->materials.at(matIdx);
			const int materialId = static_cast<int>(matIdx);

			// [rika]: if it's unloaded, or unused we will just write a stub material
			if (!materialData.asset)
			{
				rmaxFile.AddMaterial(materialData.name);
				continue;
			}

			assert(materialData.GetMaterialAsset());
			const MaterialAsset* const matlAsset = materialData.GetMaterialAsset();
			rmaxFile.AddMaterial(matlAsset->name);

			// [rika]: write stub material (unused material)
			if (!materials.contains(materialId) || !matlAsset->resourceBindings.size())
				continue;

			const ModelMaterialExport_t& materialExport = materials.find(materialId)->second;

			rmax::RMAXMaterial* const matl = rmaxFile.GetMaterialLast();

			for (const TextureAssetEntry_t& entry : matlAsset->txtrAssets)
			{
				// [rika]: we don't have a resource binding or we don't have a name for the texture
				if (!matlAsset->resourceBindings.count(entry.index) || !materialExport.textures.contains(entry.index))
					continue;

				const std::string resource = matlAsset->resourceBindings.find(entry.index)->second.name;

				// [rika]: do we need this resource?
				if (!rmax::s_TextureTypeMap.count(resource))
					continue;

				const MaterialTextureExportInfo_s& info = materialExport.textures.find(entry.index)->second;
				const std::string path = std::format("{}/{}", info.exportPath.string(), info.exportName);

				matl->AddTexture(path.c_str(), rmax::s_TextureTypeMap.find(resource)->second);
			}
		}

		// do models
		rmaxFile.ReserveCollections(lodData.models.size());
		rmaxFile.ReserveMeshes(lodData.meshes.size());
		rmaxFile.ReserveVertices(lodData.vertexCount, lodData.texcoordsPerVert, lodData.weightsPerVert);
		rmaxFile.ReserveIndices(lodData.indexCount);
		for (auto& model : lodData.models)
		{
			if (!model.meshCount)
				continue;

			rmaxFile.AddCollection(model.name.c_str(), model.meshCount);

			for (uint32_t meshIdx = 0; meshIdx < model.meshCount; meshIdx++)
			{
				const ModelMeshData_t& meshData = model.meshes[meshIdx];

				assertm(materials.contains(meshData.materialId), "material should be parsed as it is used");
				const ModelMaterialExport_t& material = materials.find(meshData.materialId)->second;

				assertm(meshData.meshVertexDataIndex != invalidNoodleIdx, "mesh data hasn't been parsed ??");

				std::unique_ptr<char[]> parsedVertexDataBuf = parsedData->meshVertexData.getIdx(meshData.meshVertexDataIndex);
				const CMeshData* const parsedVertexData = reinterpret_cast<CMeshData*>(parsedVertexDataBuf.get());

				rmaxFile.AddMesh(static_cast<int16_t>(rmaxFile.CollectionCount() - 1), static_cast<int16_t>(material.id), meshData.texcoordCount, meshData.texcoodIndices, (meshData.rawVertexLayoutFlags & VERT_COLOR));

				rmax::RMAXMesh* const mesh = rmaxFile.GetMeshLast();

				// data parsing
				for (uint32_t i = 0; i < meshData.vertCount; i++)
				{
					const Vertex_t* const vertData = &parsedVertexData->GetVertices()[i];

					Vector normal;
					Vector tangent;
					vertData->normalPacked.UnpackNormal(normal, tangent);

					mesh->AddVertex(vertData->position, normal);

					if (meshData.rawVertexLayoutFlags & VERT_COLOR)
						mesh->AddColor(vertData->color);

					for (uint16_t texcoordIdx = 0; texcoordIdx < meshData.texcoordCount; texcoordIdx++)
						mesh->AddTexcoord(*vertData->GetTexcoordForVertex(texcoordIdx, meshData.texcoordCount, parsedVertexData->GetTexcoords(), i));

					for (uint32_t weightIdx = 0; weightIdx < vertData->weightCount; weightIdx++)
					{
						const VertexWeight_t* const weight = &parsedVertexData->GetWeights()[vertData->weightIndex + weightIdx];
						mesh->AddWeight(i, weight->bone, weight->weight);
					}
				}

				for (uint32_t i = 0; i < meshData.indexCount; i += 3)
					mesh->AddIndice(parsedVertexData->GetIndices()[i], parsedVertexData->GetIndices()[i + 1], parsedVertexData->GetIndices()[i + 2]);
			}
		}

		rmaxFile.ToFile();
		rmaxFile.ResetMeshData();
	}

	return true;
}

// export parsed data to cast
// [rika]: todo rewrite this soon tm (it is so bad)
bool ExportModelCast(const ModelParsedData_t* const parsedData, std::filesystem::path& exportPath, const uint64_t guid)
{
	std::string fileNameBase = exportPath.stem().string();

	// [rika]: build the skeleton once, and reuse it
	cast::CastNode skelNode(cast::CastId::Skeleton, RTech::StringToGuid(fileNameBase.c_str()));
	{
		const size_t boneCount = parsedData->bones.size();
		skelNode.ReserveChildren(boneCount);

		// uses hashes for lookup, still gets bone parents by index :clown:
		for (size_t i = 0; i < boneCount; i++)
		{
			const ModelBone_t& boneData = parsedData->bones.at(i);

			cast::CastNodeBone boneNode(&skelNode);
			boneNode.MakeBone(boneData.name, boneData.parent, &boneData.pos, &boneData.quat, false);
		}
	}
	const cast::CastNode& skelNodeConst = skelNode;

	// [rika]: model is skin and bones, no meat
	if (parsedData->lods.size() == 0)
	{
		const std::string tmpName(std::format("{}.cast", fileNameBase));
		exportPath.replace_filename(tmpName);

		cast::CastExporter cast(exportPath.string());

		// cast
		cast::CastNode* const rootNode = cast.GetChild(0); // we only have one root node, no hash
		cast::CastNode* const modelNode = rootNode->AddChild(cast::CastId::Model, guid);

		// [rika]: we can predict how big this vector needs to be, however resizing it will make adding new members a pain.
		const size_t modelChildrenCount = 1; // skeleton (one)
		modelNode->ReserveChildren(modelChildrenCount);

		// do skeleton
		modelNode->AddChild(skelNode); // one time use

		cast.ToFile();

		return true;
	}

	const std::filesystem::path texturePath(std::format("{}/{}", exportPath.parent_path().string(), fileNameBase)); // todo, remove duplicate code
	std::unordered_map<int, ModelMaterialExport_t> materials;
	HandleModelMaterials(parsedData, materials, texturePath);

	for (size_t lodIdx = 0; lodIdx < parsedData->lods.size(); lodIdx++)
	{
		const ModelLODData_t& lodData = parsedData->lods.at(lodIdx);

		std::string tmpName(std::format("{}_LOD{}.cast", fileNameBase, std::to_string(lodIdx)));
		exportPath.replace_filename(tmpName);

		cast::CastExporter cast(exportPath.string());

		// cast
		cast::CastNode* rootNode = cast.GetChild(0); // we only have one root node, no hash
		cast::CastNode* modelNode = rootNode->AddChild(cast::CastId::Model, guid);

		// [rika]: we can predict how big this vector needs to be, however resizing it will make adding new members a pain.
		const size_t modelChildrenCount = 1 + parsedData->materials.size() + lodData.meshes.size(); // skeleton (one), materials (varies), meshes (varies)
		modelNode->ReserveChildren(modelChildrenCount);

		// do skeleton
		modelNode->AddChild(skelNodeConst);

		// do materials
		for (const auto& it : materials)
		{
			const ModelMaterialExport_t& material = it.second;
			const ModelMaterialData_t* const materialData = &parsedData->materials.at(static_cast<size_t>(material.id));

			// [rika]: a cast material has at least two properties, name and material type (pbr in our case)
			cast::CastNode matlNode(cast::CastId::Material, 2, materialData->guid);
			matlNode.SetProperty(1, cast::CastPropertyId::String, static_cast<int>(cast::CastPropsMaterial::Type), "pbr", 1u);

			if (!material.asset)
			{
				matlNode.SetProperty(0, cast::CastPropertyId::String, static_cast<int>(cast::CastPropsMaterial::Name), GetStringAfterLastSlash(materialData->name), 1u); // unsure why it does this but we're rolling with it!
				modelNode->AddChild(matlNode);
				continue;
			}

			const MaterialAsset* const materialAsset = material.asset;

			matlNode.SetProperty(0, cast::CastPropertyId::String, static_cast<int>(cast::CastPropsMaterial::Name), GetStringAfterLastSlash(materialAsset->name), 1u);

			// [rika]: parse out our textures if we have bindings for them, don't if not
			// [rika]: exit early if no textures
			if (materialAsset->resourceBindings.empty())
			{
				modelNode->AddChild(matlNode);
				continue;
			}

			for (const TextureAssetEntry_t& entry : materialAsset->txtrAssets)
			{
				// [rika]: texture cannot be accurately identified, skip it
				if (!materialAsset->resourceBindings.count(entry.index))
					continue;

				const std::string resource(materialAsset->resourceBindings.find(entry.index)->second.name);

				// [rika]: texture type isn't supported, skip it
				if (!cast::s_TextureTypeMap.count(resource))
					continue;

				// [rika]: if MaterialTextureExportInfo_s doesn't exist for this texture it's not loaded, and by extension is not exported
				if (!material.textures.contains(entry.index))
				{
					// todo: store a name in parsed data
					//Log("Material %s for model %s did not have a valid texture pointer for res idx %i\n", materialAsset->name, name, entry.index);

					continue;
				}

				const MaterialTextureExportInfo_s& info = material.textures.find(entry.index)->second;

				const uint64_t textureGuid = entry.asset->data()->guid; // texture guid

				const cast::CastPropsMaterial matlTxtrProp = cast::s_TextureTypeMap.find(resource)->second;

				matlNode.AddProperty(cast::CastPropertyId::Integer64, static_cast<int>(matlTxtrProp), textureGuid);

				cast::CastNode fileNode(cast::CastId::File, 1, textureGuid);

				// [rika]: need to figure out how this works more
				const std::string filePath(std::format("{}/{}.png", fileNameBase, info.exportName));
				fileNode.SetString(filePath); // materials exported from models always use png, as blender support for dds is bad, todo: make it so we can use ALL formats!
				fileNode.SetProperty(0, cast::CastPropertyId::String, static_cast<int>(cast::CastPropsFile::Path), fileNode.GetString(), 1u);

				matlNode.AddChild(fileNode);
			}

			modelNode->AddChild(matlNode);
		}

		// !!! TODO !!!
		// this needs to get cleaned up, but if I don't push I am gonna be stuck forever

		// [rika]: a system to have these allocated to each node would be cleaner, but more expensive.
		struct DataPtrs_t {
			Vector* positions;
			Vector* normals;
			Color32* colors;
			Vector2D* texcoords; // all uvs stored here
			uint8_t* blendIndices;
			float* blendWeights;

			uint16_t* indices;
		};

		DataPtrs_t vertexData{ nullptr };

		//                                    Postion & Normal       Color
		constexpr size_t castMinSizePerVert = (sizeof(Vector) * 2) + sizeof(Color32);

		char* vertDataBlockBuf = new char[(castMinSizePerVert + (sizeof(Vector2D) * lodData.texcoordsPerVert)) * lodData.vertexCount] {};

		vertexData.positions = reinterpret_cast<Vector*>(vertDataBlockBuf);
		vertexData.normals = &vertexData.positions[lodData.vertexCount];
		vertexData.colors = reinterpret_cast<Color32*>(&vertexData.normals[lodData.vertexCount]); // discarded if unneeded
		vertexData.texcoords = reinterpret_cast<Vector2D*>(&vertexData.colors[lodData.vertexCount]);
		vertexData.blendIndices = new uint8_t[lodData.vertexCount * lodData.weightsPerVert]{};
		vertexData.blendWeights = new float[lodData.vertexCount * lodData.weightsPerVert] {};

		vertexData.indices = new uint16_t[lodData.indexCount];

		size_t curIndex = 0; // current index into vertex data
		size_t idxIndex = 0; // shit format

		// do meshes
		for (auto& modelData : lodData.models)
		{
			for (size_t i = 0; i < modelData.meshCount; i++)
			{
				const ModelMeshData_t& meshData = modelData.meshes[i];

				assertm(materials.contains(meshData.materialId), "material should be parsed as it is used");
				const uint64_t materialGuid = parsedData->materials.at(materials.find(meshData.materialId)->second.id).guid;

				assertm(meshData.meshVertexDataIndex != invalidNoodleIdx, "mesh data hasn't been parsed ??");

				std::unique_ptr<char[]> parsedVertexDataBuf = parsedData->meshVertexData.getIdx(meshData.meshVertexDataIndex);
				const CMeshData* const parsedVertexData = reinterpret_cast<CMeshData*>(parsedVertexDataBuf.get());

				std::string matl = nullptr != meshData.materialAsset ? GetStringAfterLastSlash(meshData.GetMaterialAsset()->name) : std::to_string(materialGuid);
				std::string meshName = std::format("{}_{}", modelData.name, matl);
				cast::CastNode meshNode(cast::CastId::Mesh, 1, RTech::StringToGuid(meshName.c_str())); // name

				// name, pos, normal, blendweight, blendindices, indices, uv count, max blends, material, texcoords, color
				const size_t meshPropertiesCount = 9 + meshData.texcoordCount + (meshData.rawVertexLayoutFlags & VERT_COLOR ? 1 : 0);
				meshNode.ReserveProperties(meshPropertiesCount);

				// works on files but not here, why?
				// update: now it works after changing how the string is formed, lovely.
				meshNode.SetString(meshName);
				meshNode.SetProperty(0, cast::CastPropertyId::String, static_cast<int>(cast::CastPropsMesh::Name), meshNode.GetString(), 1u);

				meshNode.AddProperty(cast::CastPropertyId::Vector3, static_cast<int>(cast::CastPropsMesh::Vertex_Postion_Buffer), &vertexData.positions[curIndex], meshData.vertCount);
				meshNode.AddProperty(cast::CastPropertyId::Vector3, static_cast<int>(cast::CastPropsMesh::Vertex_Normal_Buffer), &vertexData.normals[curIndex], meshData.vertCount);

				if (meshData.rawVertexLayoutFlags & VERT_COLOR)
					meshNode.AddProperty(cast::CastPropertyId::Integer32, static_cast<int>(cast::CastPropsMesh::Vertex_Color_Buffer), &vertexData.colors[curIndex], meshData.vertCount);

				// cast cries if we use the proper index
				for (int16_t texcoordIdx = 0; texcoordIdx < meshData.texcoordCount; texcoordIdx++)
					meshNode.AddProperty(cast::CastPropertyId::Vector2, static_cast<int>(cast::CastPropsMesh::Vertex_UV_Buffer), &vertexData.texcoords[(lodData.vertexCount * texcoordIdx) + curIndex], meshData.vertCount, true, texcoordIdx);

				meshNode.AddProperty(cast::CastPropertyId::Byte, static_cast<int>(cast::CastPropsMesh::Vertex_Weight_Bone_Buffer), &vertexData.blendIndices[lodData.weightsPerVert * curIndex], (meshData.vertCount * meshData.weightsPerVert));
				meshNode.AddProperty(cast::CastPropertyId::Float, static_cast<int>(cast::CastPropsMesh::Vertex_Weight_Value_Buffer), &vertexData.blendWeights[lodData.weightsPerVert * curIndex], (meshData.vertCount * meshData.weightsPerVert));

				// parse our vertices into the buffer, so cringe!
				for (uint32_t vertIdx = 0; vertIdx < meshData.vertCount; vertIdx++)
				{
					const Vertex_t& vert = parsedVertexData->GetVertices()[vertIdx];

					Vector tangent;

					vertexData.positions[curIndex + vertIdx] = vert.position;
					vert.normalPacked.UnpackNormal(vertexData.normals[curIndex + vertIdx], tangent);
					vertexData.colors[curIndex + vertIdx] = vert.color;

					for (uint16_t texcoordIdx = 0; texcoordIdx < meshData.texcoordCount; texcoordIdx++)
					{
						const Vector2D* const texcoord = vert.GetTexcoordForVertex(texcoordIdx, meshData.texcoordCount, parsedVertexData->GetTexcoords(), vertIdx);
						vertexData.texcoords[(lodData.vertexCount * texcoordIdx) + curIndex + vertIdx] = *texcoord;
					}

					for (uint32_t weightIdx = 0; weightIdx < vert.weightCount; weightIdx++)
					{
						vertexData.blendIndices[(lodData.weightsPerVert * curIndex) + (meshData.weightsPerVert * vertIdx) + weightIdx] = static_cast<uint8_t>(parsedVertexData->GetWeights()[vert.weightIndex + weightIdx].bone);
						vertexData.blendWeights[(lodData.weightsPerVert * curIndex) + (meshData.weightsPerVert * vertIdx) + weightIdx] = parsedVertexData->GetWeights()[vert.weightIndex + weightIdx].weight;
					}
				}

				// parse our indices into the buffer, and shuffle them! extra cringe!
				const uint32_t indexCount = meshData.indexCount;
				meshNode.AddProperty(cast::CastPropertyId::Short, static_cast<int>(cast::CastPropsMesh::Face_Buffer), &vertexData.indices[idxIndex], indexCount);

				for (uint32_t idxIdx = 0; idxIdx < indexCount; idxIdx += 3)
				{
					vertexData.indices[idxIndex + idxIdx] = parsedVertexData->GetIndices()[idxIdx + 2];
					vertexData.indices[idxIndex + idxIdx + 1] = parsedVertexData->GetIndices()[idxIdx + 1];
					vertexData.indices[idxIndex + idxIdx + 2] = parsedVertexData->GetIndices()[idxIdx];
				}

				meshNode.AddProperty(cast::CastPropertyId::Short, static_cast<int>(cast::CastPropsMesh::UV_Layer_Count), meshData.texcoordCount);
				meshNode.AddProperty(cast::CastPropertyId::Short, static_cast<int>(cast::CastPropsMesh::Max_Weight_Influence), meshData.weightsPerVert);

				meshNode.AddProperty(cast::CastPropertyId::Integer64, static_cast<int>(cast::CastPropsMesh::Material), materialGuid);

				modelNode->AddChild(meshNode);

				curIndex += meshData.vertCount;
				idxIndex += indexCount;
			}
		}

		cast.ToFile();

		// cleanup our allocated buffers
		delete[] vertDataBlockBuf;
		delete[] vertexData.blendIndices;
		delete[] vertexData.blendWeights;

		delete[] vertexData.indices;
	}

	return true;
}

// parse a Vertex_t into a smd vertex
inline void ParseVertexIntoSMD(const Vertex_t* const srcVert, const VertexWeight_t* const srcWeights, smd::Vertex* const vert, const bool isStaticProp, const uint32_t texcoordWidth = 1u, const Vector2D* const extraTexcoords = nullptr, const uint32_t vertexIndex = 0u)
{
	Vector tangent;
	vert->position = srcVert->position;
	srcVert->normalPacked.UnpackNormal(vert->normal, tangent);

	if (isStaticProp)
	{
		StaticPropFlipFlop(vert->position);
		StaticPropFlipFlop(vert->normal);
	}

	vert->numTexcoords = texcoordWidth > smd::maxTexcoords ? smd::maxTexcoords : texcoordWidth;
	for (uint32_t texcoordIdx = 0u; texcoordIdx < vert->numTexcoords; texcoordIdx++)
	{
		vert->texcoords[texcoordIdx] = *srcVert->GetTexcoordForVertex(texcoordIdx, texcoordWidth, extraTexcoords, vertexIndex);
		Vertex_t::InvertTexcoord(vert->texcoords[texcoordIdx]); // [rika]: the texcoord has to be inverted for proper recompile
	}

	vert->numBones = srcVert->weightCount > smd::maxBoneWeights ? smd::maxBoneWeights : srcVert->weightCount;
	for (uint32_t weightIdx = 0u; weightIdx < vert->numBones; weightIdx++)
	{
		const VertexWeight_t* const weight = &srcWeights[srcVert->weightIndex + weightIdx];

		vert->bone[weightIdx] = weight->bone;
		vert->weight[weightIdx] = weight->weight;
	}
}

// export parsed data into smd files
bool ExportModelSMD(const ModelParsedData_t* const parsedData, std::filesystem::path& exportPath)
{
	std::string fileNameBase = exportPath.stem().string();
	const std::filesystem::path filePath(exportPath.parent_path());

	smd::CStudioModelData* const smd = new smd::CStudioModelData(filePath, parsedData->bones.size(), 1ull);

	// [rika]: initialize the nodes, and in this case the frames since we should only have one
	for (size_t i = 0; i < parsedData->bones.size(); i++)
	{
		const ModelBone_t& bone = parsedData->bones.at(i);
		const int ibone = static_cast<int>(i);

		smd->InitNode(bone.name, ibone, bone.parent);
		smd->InitFrameBone(0, ibone, bone.pos, bone.rot);
	}

	// [rika]: model is skin and bones, no meat
	if (parsedData->lods.size() == 0)
	{
		smd->SetName(fileNameBase);
		smd->Write();

		return true;
	}

	const std::filesystem::path texturePath(std::format("{}/{}", filePath.string(), fileNameBase)); // todo, remove duplicate code
	std::unordered_map<int, ModelMaterialExport_t> materials;
	HandleModelMaterials(parsedData, materials, texturePath);

	const bool isStaticProp = parsedData->studiohdr.flags & STUDIOHDR_FLAGS_STATIC_PROP ? true : false;

	CManagedBuffer* const buf = g_BufferManager.ClaimBuffer();

	for (size_t lodIdx = 0; lodIdx < parsedData->lods.size(); lodIdx++)
	{
		const ModelLODData_t& lod = parsedData->lods.at(lodIdx);

		for (const ModelModelData_t& model : lod.models)
		{
			std::string name(model.name);
			FixupExportLodNames(name, static_cast<int>(lodIdx));

			// unique prefix so we don't overwrite files
			name = std::format("{}_{}", fileNameBase, name);
			smd->SetName(name);

			for (uint32_t meshIdx = 0; meshIdx < model.meshCount; meshIdx++)
			{
				const ModelMeshData_t& meshData = lod.meshes.at(model.meshIndex + meshIdx);

				assertm(meshData.meshVertexDataIndex != invalidNoodleIdx, "mesh data hasn't been parsed ??");

				std::unique_ptr<char[]> parsedVertexDataBuf = parsedData->meshVertexData.getIdx(meshData.meshVertexDataIndex);
				const CMeshData* const parsedVertexData = reinterpret_cast<CMeshData*>(parsedVertexDataBuf.get());

				const uint16_t* const indices = parsedVertexData->GetIndices();
				const Vertex_t* const vertices = parsedVertexData->GetVertices();
				const VertexWeight_t* const weights = parsedVertexData->GetWeights();
				const Vector2D* const texcoords = parsedVertexData->GetTexcoords();

				// [rika]: add more triangles
				smd->AddMeshCapacity(meshData.vertCount, meshData.indexCount / 3u);

				const ModelMaterialData_t* const materialData = parsedData->pMaterial(meshData.materialId);

				// [rika]: making the choice to use the stored rmdl name here when possible, as that is what it was likely compiled with
				const char* material = materialData->GetName(true);
				assertm(material, "material name should always be valid");

				material = g_ExportSettings.exportModelMatsTruncated ? material : GetStringAfterLastSlash(material);

				for (uint32_t vertexIdx = 0; vertexIdx < meshData.vertCount; vertexIdx++)
				{
					smd::Vertex vertex;
					ParseVertexIntoSMD(&vertices[vertexIdx], weights, &vertex, isStaticProp, meshData.texcoordCount, texcoords, vertexIdx);

					smd->InitVertex(&vertex);
				}

				for (uint32_t indiceIdx = 0; indiceIdx < meshData.indexCount; indiceIdx += 3)
				{
					const uint16_t indice0 = indices[indiceIdx];
					const uint16_t indice1 = indices[indiceIdx + 1];
					const uint16_t indice2 = indices[indiceIdx + 2];

					// order of indices is odd
					smd->InitLocalTriangle(material, indice0, indice2, indice1);
				}
			}

			smd->Write(buf->Buffer(), managedBufferSize);
			smd->ResetMeshData();
		}
	}

	FreeAllocVar(smd);
	g_BufferManager.RelieveBuffer(buf);

	return true;
}

// export a seqdesc to rmax
bool ExportSeqDescRMAX(const ModelSeq_t* const seqdesc, std::filesystem::path& exportPath, const char* const skelName, const std::vector<ModelBone_t>* const bones)
{
	const std::string fileNameBase = exportPath.stem().string();
	const std::string skelNameBase = std::filesystem::path(skelName).stem().string();

	const size_t boneCount = bones->size();

	for (int animIdx = 0; animIdx < seqdesc->AnimCount(); animIdx++)
	{
		const std::string animName = std::format("{}{}", fileNameBase.c_str(), animIdx);

		const std::string tmpName = std::format("{}.rmax", animName);
		exportPath.replace_filename(tmpName);

		rmax::RMAXExporter rmaxFile(exportPath, fileNameBase.c_str(), fileNameBase.c_str());

		// do bones
		rmaxFile.ReserveBones(boneCount);
		for (auto& bone : *bones)
			rmaxFile.AddBone(bone.name, bone.parent, bone.pos, bone.quat, bone.scale);

		const ModelAnim_t* const animdesc = seqdesc->anims + animIdx; // check flag 0x20000

		uint16_t animFlags = 0;

		if (animdesc->flags & eStudioAnimFlags::ANIM_DELTA) // delta flag
			animFlags |= rmax::AnimFlags_t::ANIM_DELTA;

		// [rika]: not touching this for now since we really don't care about empty bones on types not for re import
		if (!(animdesc->flags & eStudioAnimFlags::ANIM_VALID) || animdesc->parsedBufferIndex == invalidNoodleIdx)
			animFlags |= rmax::AnimFlags_t::ANIM_EMPTY;

		rmaxFile.AddAnim(animName.c_str(), static_cast<uint16_t>(animdesc->numframes), animdesc->fps, animFlags, boneCount);
		rmax::RMAXAnim* const anim = rmaxFile.GetAnimLast();

		if (anim->GetFlags() & rmax::AnimFlags_t::ANIM_EMPTY)
		{
			rmaxFile.ToFile();

			continue;
		}

		const std::unique_ptr<char[]> noodle = seqdesc->parsedData.getIdx(animdesc->parsedBufferIndex);
		CAnimData animData(noodle.get());

		for (int i = 0; i < boneCount; i++)
		{
			const uint8_t flags = animData.GetFlag(i);

			anim->SetTrack(flags, static_cast<uint16_t>(i));
			rmax::RMAXAnimTrack* const track = anim->GetTrack(i);

			const Vector* pos = nullptr;
			const Quaternion* q = nullptr;
			const Vector* scale = nullptr;

			if (flags & CAnimDataBone::ANIMDATA_POS)
				pos = animData.GetBonePosForFrame(i, 0);

			if (flags & CAnimDataBone::ANIMDATA_ROT)
				q = animData.GetBoneQuatForFrame(i, 0);

			if (flags & CAnimDataBone::ANIMDATA_SCL)
				scale = animData.GetBoneScaleForFrame(i, 0);

			for (int frameIdx = 0; frameIdx < animdesc->numframes; frameIdx++)
			{
				track->AddFrame(frameIdx, &pos[frameIdx], &q[frameIdx], &scale[frameIdx]);
			}
		}

		rmaxFile.ToFile();
	}

	return true;
}

// export a seq desc to cast
bool ExportSeqDescCast(const ModelSeq_t* const seqdesc, std::filesystem::path& exportPath, const char* const skelName, const std::vector<ModelBone_t>* const bones, const uint64_t guid)
{
	const std::string fileNameBase = exportPath.stem().string();
	const std::string skelNameBase = std::filesystem::path(skelName).stem().string();

	const size_t boneCount = bones->size();

	for (int animIdx = 0; animIdx < seqdesc->AnimCount(); animIdx++)
	{
		const ModelAnim_t* const animdesc = seqdesc->anims + animIdx;

		const std::string tmpName(std::format("{}_{}.cast", fileNameBase, std::to_string(animIdx)));
		exportPath.replace_filename(tmpName);

		cast::CastExporter cast(exportPath.string());

		// cast
		cast::CastNode* const rootNode = cast.GetChild(0); // we only have one root node, no hash
		cast::CastNode* const animNode = rootNode->AddChild(cast::CastId::Animation, guid);

		// [rika]: we can predict how big this vector needs to be, however resizing it will make adding new members a pain.
		const size_t animChildrenCount = 1 + (boneCount * 7); // skeleton (one), curve per bone per data type
		animNode->ReserveChildren(animChildrenCount);
		animNode->ReserveProperties(2); // framerate, looping

		animNode->AddProperty(cast::CastPropertyId::Float, static_cast<int>(cast::CastPropsAnimation::Framerate), FLOAT_AS_UINT(animdesc->fps));
		animNode->AddProperty(cast::CastPropertyId::Byte, static_cast<int>(cast::CastPropsAnimation::Looping), animdesc->flags & eStudioAnimFlags::ANIM_LOOPING ? true : false);

		// do skeleton
		{
			// it would be more ideal to just feed it bones, but I don't want to deal with that mess of functions currently
			cast::CastNode* const skelNode = animNode->AddChild(cast::CastId::Skeleton, RTech::StringToGuid(fileNameBase.c_str()));
			skelNode->ReserveChildren(boneCount);

			// uses hashes for lookup, still gets bone parents by index :clown:
			for (size_t i = 0; i < boneCount; i++)
			{
				const ModelBone_t* const boneData = &bones->at(i);

				cast::CastNodeBone boneNode(skelNode);
				boneNode.MakeBone(boneData->name, boneData->parent, &boneData->pos, &boneData->quat, false);
			}
		}

		// [rika]: not touching this for now since we really don't care about empty bones on types not for re import
		if (!(animdesc->flags & eStudioAnimFlags::ANIM_VALID) || animdesc->parsedBufferIndex == invalidNoodleIdx)
		{
			cast.ToFile();

			continue;
		}

		const std::unique_ptr<char[]> noodle = seqdesc->parsedData.getIdx(animdesc->parsedBufferIndex);
		CAnimData animData(noodle.get());

		const cast::CastPropsCurveMode curveMode = animdesc->flags & eStudioAnimFlags::ANIM_DELTA ? cast::CastPropsCurveMode::MODE_ADDITIVE : cast::CastPropsCurveMode::MODE_ABSOLUTE;

		// setup the stupid key frame buffer thing that cast curves use
		cast::CastPropertyId frameBufferId;
		void* const frameBuffer = cast::CastNodeCurve::MakeCurveKeyFrameBuffer(static_cast<size_t>(animdesc->numframes), frameBufferId);

		Vector deltaPos(0.0f, 0.0f, 0.0f);
		Quaternion deltaQuat(0.0f, 0.0f, 0.0f, 1.0f);
		Vector deltaScale(1.0f, 1.0f, 1.0f);

		for (int i = 0; i < boneCount; i++)
		{
			const ModelBone_t* const boneData = &bones->at(i);

			// parsed data
			const uint8_t flags = animData.GetFlag(i);

			// weight for delta anims
			const float animWeight = seqdesc->weight(i);

			if (flags & CAnimDataBone::ANIMDATA_POS)
			{
				cast::CastNodeCurve curveNode(animNode);
				curveNode.MakeCurveVector(boneData->name, animData.GetBonePosForFrame(i, 0), static_cast<size_t>(animdesc->numframes), frameBuffer, static_cast<size_t>(animdesc->numframes), cast::CastPropsCurveValue::POS_X, curveMode, animWeight);
			}
			else
			{
				const Vector* const track = animdesc->flags & eStudioAnimFlags::ANIM_DELTA ? &deltaPos : &boneData->pos;

				cast::CastNodeCurve curveNode(animNode);
				curveNode.MakeCurveVector(boneData->name, track, 1ull, frameBuffer, static_cast<size_t>(animdesc->numframes), cast::CastPropsCurveValue::POS_X, curveMode, animWeight);
			}

			if (flags & CAnimDataBone::ANIMDATA_ROT)
			{
				cast::CastNodeCurve curveNode(animNode);
				curveNode.MakeCurveQuaternion(boneData->name, animData.GetBoneQuatForFrame(i, 0), static_cast<size_t>(animdesc->numframes), frameBuffer, static_cast<size_t>(animdesc->numframes), curveMode, animWeight);
			}
			else
			{
				const Quaternion* const track = animdesc->flags & eStudioAnimFlags::ANIM_DELTA ? &deltaQuat : &boneData->quat;

				cast::CastNodeCurve curveNode(animNode);
				curveNode.MakeCurveQuaternion(boneData->name, track, 1ull, frameBuffer, static_cast<size_t>(animdesc->numframes), curveMode, animWeight);
			}

			// check if the sequence has scale data.
			if (seqdesc->flags & 0x20000)
			{
				if (flags & CAnimDataBone::ANIMDATA_SCL)
				{
					cast::CastNodeCurve curveNode(animNode);
					curveNode.MakeCurveVector(boneData->name, animData.GetBoneScaleForFrame(i, 0), static_cast<size_t>(animdesc->numframes), frameBuffer, static_cast<size_t>(animdesc->numframes), cast::CastPropsCurveValue::SCL_X, curveMode, animWeight);
				}
				else
				{
					const Vector* track = animdesc->flags & eStudioAnimFlags::ANIM_DELTA ? &deltaScale : &boneData->scale;

					cast::CastNodeCurve curveNode(animNode);
					curveNode.MakeCurveVector(boneData->name, track, 1ull, frameBuffer, static_cast<size_t>(animdesc->numframes), cast::CastPropsCurveValue::SCL_X, curveMode, animWeight);
				}
			}
		}

		cast.ToFile();

		delete[] frameBuffer;
	}

	return true;
}

bool ExportSeqDescSMD(const ModelSeq_t* const seqdesc, std::filesystem::path& exportPath, const char* const skelName, const std::vector<ModelBone_t>* const bones)
{
	const std::string fileNameBase = exportPath.stem().string();
	const std::string skelNameBase = std::filesystem::path(skelName).stem().string();

	const size_t boneCount = bones->size();

	smd::CStudioModelData* const smd = new smd::CStudioModelData(exportPath.parent_path(), bones->size(), 1ull);

	// [rika]: initialize the nodes
	for (size_t i = 0; i < boneCount; i++)
	{
		const ModelBone_t& bone = bones->at(i);

		smd->InitNode(bone.name, static_cast<int>(i), bone.parent);
	}

	const Vector deltaPos(0.0f, 0.0f, 0.0f);
	const Quaternion deltaQuat(0.0f, 0.0f, 0.0f, 1.0f);

	CManagedBuffer* const buf = g_BufferManager.ClaimBuffer();

	for (int animIdx = 0; animIdx < seqdesc->AnimCount(); animIdx++)
	{
		const ModelAnim_t* const animdesc = seqdesc->anims + animIdx;

		assertm(animdesc->name, "name was nullptr");
		std::string animname(animdesc->pszName());
		animname.append(".smd");

		smd->ResetFrameData(static_cast<size_t>(animdesc->numframes));
		smd->SetName(animname);

		if (animdesc->parsedBufferIndex == invalidNoodleIdx)
		{
			smd->Write();

			continue;
		}

		const std::unique_ptr<char[]> noodle = seqdesc->parsedData.getIdx(animdesc->parsedBufferIndex);
		CAnimData animData(noodle.get());

		for (int frame = 0; frame < animdesc->numframes; frame++)
		{
			for (int bone = 0; bone < boneCount; bone++)
			{
				const ModelBone_t* const boneData = &bones->at(bone);

				// parsed data
				const uint8_t flags = animData.GetFlag(bone);

				const Vector* pos = nullptr;
				const Quaternion* q = nullptr;

				if (flags & CAnimDataBone::ANIMDATA_POS)
					pos = animData.GetBonePosForFrame(bone, frame);
				else
					pos = animdesc->flags & eStudioAnimFlags::ANIM_DELTA ? &deltaPos : &boneData->pos;

				if (flags & CAnimDataBone::ANIMDATA_ROT)
					q = animData.GetBoneQuatForFrame(bone, frame);
				else
					q = animdesc->flags & eStudioAnimFlags::ANIM_DELTA ? &deltaQuat : &boneData->quat;

				assertm(pos, "should not be nullptr");
				assertm(q, "should not be nullptr");

				const RadianEuler rot(*q);

				smd->InitFrameBone(frame, bone, *pos, rot);
			}
		}

		smd->Write(buf->Buffer(), managedBufferSize);
	}

	FreeAllocVar(smd);
	g_BufferManager.RelieveBuffer(buf);

	return true;
}

bool ExportSeqDesc(const int setting, const ModelSeq_t* const seqdesc, std::filesystem::path& exportPath, const char* const skelName, const std::vector<ModelBone_t>* const bones, const uint64_t guid)
{
	switch (setting)
	{

	case eAnimSeqExportSetting::ANIMSEQ_CAST:
	{
		return ExportSeqDescCast(seqdesc, exportPath, skelName, bones, guid);
	}
	case eAnimSeqExportSetting::ANIMSEQ_RMAX:
	{
		return ExportSeqDescRMAX(seqdesc, exportPath, skelName, bones);
	}
	case eAnimSeqExportSetting::ANIMSEQ_SMD:
	{
		return ExportSeqDescSMD(seqdesc, exportPath, skelName, bones);
	}
	case eAnimSeqExportSetting::ANIMSEQ_RSEQ:
	{
		return false;
	}
	default:
	{
		assertm(false, "Export setting is not handled.");
		return false;
	}
	}
}

void CalcMatrixForBone_Unparented(const ModelBone_t& bone, matrix3x4_t& matOut)
{
	matrix3x4_t mat;
	QuaternionMatrix(bone.quat, mat);
	MatrixSetColumn(bone.pos, 3, mat);

	mat[0][0] *= bone.scale.x;
	mat[1][0] *= bone.scale.x;
	mat[2][0] *= bone.scale.x;
	mat[0][1] *= bone.scale.y;
	mat[1][1] *= bone.scale.y;
	mat[2][1] *= bone.scale.y;
	mat[0][2] *= bone.scale.z;
	mat[1][2] *= bone.scale.z;
	mat[2][2] *= bone.scale.z;

	matOut = mat;
}

void CalcMatrixForBone_Unparented(const ModelBone_t& bone, XMMATRIX& matOut)
{
	XMVECTOR quat = { bone.quat.x, bone.quat.y, bone.quat.z, bone.quat.w };
	XMVECTOR pos = { bone.pos.x, bone.pos.y, bone.pos.z };

	XMMATRIX rotationMatrix = XMMatrixRotationQuaternion(quat);

	XMMATRIX translationMatrix = XMMatrixTranslation(bone.pos.x, bone.pos.y, bone.pos.z);

	XMMATRIX transform = XMMatrixMultiply(rotationMatrix, translationMatrix);

	XMMATRIX finalMatrix = XMMatrixMultiply(transform, XMMatrixScaling(bone.scale.x, bone.scale.y, bone.scale.z));

	matOut = finalMatrix;
}

//
// PREVIEWDATA
//
extern PreviewSettings_t g_PreviewSettings;

namespace
{

	struct PreviewSequenceState_t
	{
		const ModelSeq_t* sequence = nullptr;
		const ModelAnim_t* animation = nullptr;
		const ModelParsedData_t* skeleton = nullptr;
	};

	struct PreviewAttachmentWorld_t
	{
		const ModelAttachment_t* attachment = nullptr;
		matrix3x4_t worldMatrix{};
	};

	std::string GetLowerStem(const std::string& pathStr)
	{
		std::filesystem::path p(pathStr);
		std::string stem = p.stem().string();
		for (char& c : stem)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return stem;
	}

	void DispatchBodygroupEvent(ModelPreviewInfo_t* const info, const ModelEvent_t& ev, ModelParsedData_t* const meshParsedData, bool enable)
	{
		if (!meshParsedData)
			return;

		std::string options = ParseBodygroupEventOptions(ev.options);
		if (options.empty())
			return;

		std::string partName = options;
		int value = enable ? 1 : 0;

		size_t spacePos = options.find_first_of(" \t");
		if (spacePos != std::string::npos)
		{
			partName = options.substr(0, spacePos);
			std::string valStr = options.substr(spacePos + 1);
			valStr.erase(0, valStr.find_first_not_of(" \t"));
			if (!valStr.empty() && std::isdigit(static_cast<unsigned char>(valStr[0])))
			{
				value = std::stoi(valStr);
			}
		}

		for (size_t i = 0; i < meshParsedData->bodyParts.size(); ++i)
		{
			auto& bodypart = meshParsedData->bodyParts[i];
			if (bodypart.partName == partName)
			{
				bodypart.previewEnabled = enable;
				if (info && i < info->bodygroupModelSelected.size())
				{
					int numModels = bodypart.numModels;
					if (value >= 0 && value < numModels)
					{
						info->bodygroupModelSelected[i] = value;
					}
					else if (!enable)
					{
						info->bodygroupModelSelected[i] = 0;
					}
				}
				break;
			}
		}
	}

	void DispatchCreatePropEvent(ModelPreviewInfo_t* const info, const ModelEvent_t& ev)
	{
		ParsedCreatePropEvent_t parsed = ParseCreatePropOptions(ev.options);
		if (!parsed.valid)
			return;

		for (const auto& existing : info->spawnedProps)
		{
			if (existing.attachmentName == parsed.attachment && existing.seqPath == parsed.seqPath)
				return;
		}

		std::string targetModelStem = GetLowerStem(parsed.modelPath);
		std::string targetSeqStem = GetLowerStem(parsed.seqPath);

		CPakAsset* modelAsset = nullptr;
		CPakAsset* seqAsset = nullptr;

		for (auto& lookup : g_assetData.v_assets)
		{
			CAsset* const candidateAsset = lookup.m_asset;
			if (!candidateAsset || candidateAsset->GetAssetContainerType() != CAsset::ContainerType::PAK)
				continue;

			CPakAsset* const candidate = static_cast<CPakAsset*>(candidateAsset);
			if (!candidate->hasExtraData())
				continue;

			if (candidate->GetAssetType() == '_ldm')
			{
				if (!candidate->GetAssetName().empty() && GetLowerStem(candidate->GetAssetName()) == targetModelStem)
					modelAsset = candidate;
			}
			else if (candidate->GetAssetType() == 'qesa')
			{
				if (!candidate->GetAssetName().empty() && GetLowerStem(candidate->GetAssetName()) == targetSeqStem)
					seqAsset = candidate;
			}

			if (modelAsset && seqAsset)
				break;
		}

		if (!modelAsset || !seqAsset)
			return;

		AnimSeqAsset* const seqData = seqAsset->extraData<AnimSeqAsset*>();
		if (!seqData || !seqData->animationParsed)
			return;

		PropEntity_t prop{};
		prop.modelGuid = modelAsset->GetAssetGUID();
		prop.modelPath = modelAsset->GetAssetName();
		prop.seqGuid = seqAsset->GetAssetGUID();
		prop.attachmentName = parsed.attachment;
		prop.seqPath = parsed.seqPath;
		prop.spawnCycle = ev.cycle;
		prop.looping = (seqData->seqdesc.flags & STUDIO_LOOPING) != 0;

		info->spawnedProps.push_back(std::move(prop));
	}

	void CalcMatrixForBone_Unparented(const PreviewBonePose_t& pose, XMMATRIX& matOut)
	{
		XMVECTOR quat = { pose.quat.x, pose.quat.y, pose.quat.z, pose.quat.w };

		XMMATRIX rotationMatrix = XMMatrixRotationQuaternion(quat);
		XMMATRIX translationMatrix = XMMatrixTranslation(pose.pos.x, pose.pos.y, pose.pos.z);
		XMMATRIX transform = XMMatrixMultiply(rotationMatrix, translationMatrix);
		XMMATRIX finalMatrix = XMMatrixMultiply(transform, XMMatrixScaling(pose.scale.x, pose.scale.y, pose.scale.z));

		matOut = finalMatrix;
	}

	void ClearDebugPrimitives(CDXDrawData* const drawData)
	{
		for (DXMeshDrawData_DebugPrim_t& prim : drawData->debugPrims)
		{
			DX_RELEASE_PTR(prim.vertexBuffer);
			DX_RELEASE_PTR(prim.indexBuffer);
		}

		drawData->debugPrims.clear();
	}

	void BuildBoneNameRemap(std::vector<int>& remap, const std::vector<ModelBone_t>& targetBones, const std::vector<ModelBone_t>& sourceBones)
	{
		std::unordered_map<std::string_view, int> sourceByName;
		sourceByName.reserve(sourceBones.size());

		for (size_t i = 0; i < sourceBones.size(); ++i)
			sourceByName.emplace(sourceBones[i].name, static_cast<int>(i));

		remap.assign(targetBones.size(), -1);
		for (size_t i = 0; i < targetBones.size(); ++i)
		{
			const auto it = sourceByName.find(targetBones[i].name);
			if (it != sourceByName.end())
				remap[i] = it->second;
		}
	}

	int ResolvePreviewAnimationIndex(const ModelPreviewInfo_t* const info, const ModelSeq_t* const seqdesc)
	{
		if (!seqdesc || !seqdesc->AnimCount())
			return -1;

		if (info && info->selectedAnimationIndex >= 0 && info->selectedAnimationIndex < seqdesc->AnimCount())
			return info->selectedAnimationIndex;

		if (seqdesc->numblends > 0 && seqdesc->blends)
		{
			const int blendIndex = seqdesc->blends[0];
			if (blendIndex >= 0 && blendIndex < seqdesc->AnimCount())
				return blendIndex;
		}

		return 0;
	}

	Quaternion MultiplyQuaternion(const Quaternion& lhs, const Quaternion& rhs)
	{
		Quaternion result;
		QuaternionMult(lhs, rhs, result);
		return result;
	}

	Vector ConvertPositionToPreviewSpace(const Vector& sourcePosition)
	{
		return { sourcePosition.x, sourcePosition.z, sourcePosition.y };
	}

	Vector ConvertDirectionToPreviewSpace(const Vector& sourceDirection)
	{
		return { sourceDirection.x, sourceDirection.z, sourceDirection.y };
	}

	Vector GetPreviewCameraPosition()
	{
		const XMMATRIX invView = XMMatrixInverse(nullptr, g_dxHandler->GetCamera()->GetViewMatrix());
		return {
			invView.r[3].m128_f32[0],
			invView.r[3].m128_f32[1],
			invView.r[3].m128_f32[2]
		};
	}

	bool ShouldCullPreviewDebugLine(const Vector& start, const Vector& end)
	{
		constexpr float kCullDistance = 1.5f;
		const float cullDistanceSqr = kCullDistance * kCullDistance;

		const Vector cameraPos = GetPreviewCameraPosition();
		const Vector segment = end - start;
		const float segmentLengthSqr = Vector::Dot(segment, segment);

		float t = 0.0f;
		if (segmentLengthSqr > FLT_EPSILON)
			t = std::clamp(Vector::Dot(cameraPos - start, segment) / segmentLengthSqr, 0.0f, 1.0f);

		const Vector closestPoint = start + (segment * t);
		const Vector cameraToSegment = closestPoint - cameraPos;
		return Vector::Dot(cameraToSegment, cameraToSegment) < cullDistanceSqr;
	}

	XMMATRIX GetPreviewBasisMatrix()
	{
		return XMMATRIX(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		);
	}

	XMMATRIX ConvertSkinningMatrixToPreviewSpace(const XMMATRIX& sourceMatrix)
	{
		const XMMATRIX previewBasis = GetPreviewBasisMatrix();
		return XMMatrixMultiply(XMMatrixMultiply(previewBasis, sourceMatrix), previewBasis);
	}

	XMMATRIX Matrix3x4ToXMMATRIX(const matrix3x4_t& matrix)
	{
		return XMMATRIX(
			matrix[0][0], matrix[0][1], matrix[0][2], matrix[0][3],
			matrix[1][0], matrix[1][1], matrix[1][2], matrix[1][3],
			matrix[2][0], matrix[2][1], matrix[2][2], matrix[2][3],
			0.0f, 0.0f, 0.0f, 1.0f
		);
	}

	Vector TransformPointByMatrix(const matrix3x4_t& matrix, const Vector& point)
	{
		return {
			(point.x * matrix[0][0]) + (point.y * matrix[0][1]) + (point.z * matrix[0][2]) + matrix[0][3],
			(point.x * matrix[1][0]) + (point.y * matrix[1][1]) + (point.z * matrix[1][2]) + matrix[1][3],
			(point.x * matrix[2][0]) + (point.y * matrix[2][1]) + (point.z * matrix[2][2]) + matrix[2][3]
		};
	}

	void BuildWorldBoneMatrices(const std::vector<ModelBone_t>& bones, const std::vector<PreviewBonePose_t>& localPose, std::vector<matrix3x4_t>& worldMatrices)
	{
		worldMatrices.resize(bones.size());
		for (size_t i = 0; i < bones.size(); ++i)
		{
			QuaternionMatrix(localPose[i].quat, localPose[i].pos, worldMatrices[i]);
			worldMatrices[i][0][0] *= localPose[i].scale.x;
			worldMatrices[i][1][0] *= localPose[i].scale.x;
			worldMatrices[i][2][0] *= localPose[i].scale.x;
			worldMatrices[i][0][1] *= localPose[i].scale.y;
			worldMatrices[i][1][1] *= localPose[i].scale.y;
			worldMatrices[i][2][1] *= localPose[i].scale.y;
			worldMatrices[i][0][2] *= localPose[i].scale.z;
			worldMatrices[i][1][2] *= localPose[i].scale.z;
			worldMatrices[i][2][2] *= localPose[i].scale.z;

			const int parent = bones[i].parent;
			if (parent >= 0)
			{
				matrix3x4_t concatenated{};
				ConcatTransforms(worldMatrices[parent], worldMatrices[i], concatenated);
				worldMatrices[i] = concatenated;
			}
		}
	}

	void BuildAttachmentWorldTransforms(const ModelParsedData_t* const parsedData, const std::vector<matrix3x4_t>& boneWorldMatrices, std::vector<PreviewAttachmentWorld_t>& outAttachments)
	{
		outAttachments.clear();
		if (!parsedData || parsedData->attachments.empty())
			return;

		outAttachments.reserve(parsedData->attachments.size());
		for (const ModelAttachment_t& attachment : parsedData->attachments)
		{
			if (!attachment.localmatrix || attachment.localbone < 0 || static_cast<size_t>(attachment.localbone) >= boneWorldMatrices.size())
				continue;

			PreviewAttachmentWorld_t worldAttachment{};
			worldAttachment.attachment = &attachment;
			ConcatTransforms(boneWorldMatrices[attachment.localbone], *attachment.localmatrix, worldAttachment.worldMatrix);
			outAttachments.push_back(worldAttachment);
		}
	}

	const PreviewAttachmentWorld_t* FindPreviewAttachmentByName(const std::vector<PreviewAttachmentWorld_t>& attachments, const char* const name)
	{
		if (!name || !name[0])
			return nullptr;

		for (const PreviewAttachmentWorld_t& attachment : attachments)
		{
			if (attachment.attachment && attachment.attachment->name && !strcmp(attachment.attachment->name, name))
				return &attachment;
		}

		return nullptr;
	}

	void DrawPreviewAttachments(CDXDrawData* const drawData, const std::vector<PreviewAttachmentWorld_t>& attachments)
	{
		for (const PreviewAttachmentWorld_t& attachment : attachments)
		{
			if (!attachment.attachment)
				continue;

			Vector origin{};
			MatrixPosition(attachment.worldMatrix, origin);

			const Vector sourceX = TransformPointByMatrix(attachment.worldMatrix, { 2.0f, 0.0f, 0.0f });
			const Vector sourceY = TransformPointByMatrix(attachment.worldMatrix, { 0.0f, 2.0f, 0.0f });
			const Vector sourceZ = TransformPointByMatrix(attachment.worldMatrix, { 0.0f, 0.0f, 2.0f });

			const Vector previewOrigin = ConvertPositionToPreviewSpace(origin);
			const Vector previewX = ConvertPositionToPreviewSpace(sourceX);
			const Vector previewY = ConvertPositionToPreviewSpace(sourceY);
			const Vector previewZ = ConvertPositionToPreviewSpace(sourceZ);

			if (!ShouldCullPreviewDebugLine(previewOrigin, previewX))
				drawData->DrawLine(previewOrigin, previewX, 0xFFFF8000, false, 1.0f, -1.0f);
			if (!ShouldCullPreviewDebugLine(previewOrigin, previewY))
				drawData->DrawLine(previewOrigin, previewY, 0xFFFF8000, false, 1.0f, -1.0f);
			if (!ShouldCullPreviewDebugLine(previewOrigin, previewZ))
				drawData->DrawLine(previewOrigin, previewZ, 0xFFFF8000, false, 1.0f, -1.0f);
		}
	}

	void DisableAttachedPreviewCamera()
	{
		g_PreviewSettings.previewUseAttachedCamera = false;
		g_PreviewSettings.previewFovDegrees = 45.0f;
		g_dxHandler->UpdateProjectionMatrix();
	}

	void ApplyPreviewCameraAttachment(const PreviewAttachmentWorld_t& attachment)
	{
		Vector origin{};
		MatrixPosition(attachment.worldMatrix, origin);

		const Vector sourceForwardPoint = TransformPointByMatrix(attachment.worldMatrix, { 8.0f, 0.0f, 0.0f });
		const Vector sourceUpPoint = TransformPointByMatrix(attachment.worldMatrix, { 0.0f, 0.0f, 8.0f });

		const Vector previewOrigin = ConvertPositionToPreviewSpace(origin);
		const Vector previewTarget = ConvertPositionToPreviewSpace(sourceForwardPoint);
		Vector previewUp = ConvertDirectionToPreviewSpace(sourceUpPoint - origin);
		const float previewUpLengthSqr = Vector::Dot(previewUp, previewUp);
		if (previewUpLengthSqr <= FLT_EPSILON)
			previewUp = { 0.0f, 1.0f, 0.0f };
		else
			previewUp /= std::sqrt(previewUpLengthSqr);

		g_PreviewSettings.previewUseAttachedCamera = true;
		g_PreviewSettings.previewFovDegrees = 110.0f;
		g_PreviewSettings.previewAttachedCameraOriginX = previewOrigin.x;
		g_PreviewSettings.previewAttachedCameraOriginY = previewOrigin.y;
		g_PreviewSettings.previewAttachedCameraOriginZ = previewOrigin.z;
		g_PreviewSettings.previewAttachedCameraTargetX = previewTarget.x;
		g_PreviewSettings.previewAttachedCameraTargetY = previewTarget.y;
		g_PreviewSettings.previewAttachedCameraTargetZ = previewTarget.z;
		g_PreviewSettings.previewAttachedCameraUpX = previewUp.x;
		g_PreviewSettings.previewAttachedCameraUpY = previewUp.y;
		g_PreviewSettings.previewAttachedCameraUpZ = previewUp.z;
		g_dxHandler->UpdateProjectionMatrix();
	}

	bool EvaluateAnimationPose(const ModelSeq_t* const seqdesc, const ModelParsedData_t* const skeleton, const ModelAnim_t* const animdesc, const int frameIndex, std::vector<PreviewBonePose_t>& outPose, const std::vector<PreviewBonePose_t>* basePose = nullptr)
	{
		if (!seqdesc || !skeleton || !animdesc)
			return false;

		const size_t boneCount = skeleton->bones.size();
		outPose.resize(boneCount);
		for (size_t i = 0; i < boneCount; ++i)
		{
			const ModelBone_t& bone = skeleton->bones[i];
			outPose[i].pos = bone.pos;
			outPose[i].quat = bone.quat;
			outPose[i].scale = bone.scale;
		}

		if (!(animdesc->flags & eStudioAnimFlags::ANIM_VALID) || animdesc->parsedBufferIndex == invalidNoodleIdx)
			return true;

		const std::unique_ptr<char[]> noodle = seqdesc->parsedData.getIdx(animdesc->parsedBufferIndex);
		if (!noodle)
			return true;

		CAnimData animData(noodle.get());
		const bool isDelta = (animdesc->flags & eStudioAnimFlags::ANIM_DELTA) != 0;

		for (size_t i = 0; i < boneCount; ++i)
		{
			const ModelBone_t& bone = skeleton->bones[i];
			const uint8_t flags = animData.GetFlag(i);

			if (isDelta)
			{
				const Vector& basePos = (basePose && i < basePose->size()) ? (*basePose)[i].pos : bone.pos;
				const Quaternion& baseQuat = (basePose && i < basePose->size()) ? (*basePose)[i].quat : bone.quat;
				const Vector& baseScale = (basePose && i < basePose->size()) ? (*basePose)[i].scale : bone.scale;

				outPose[i].pos = basePos;
				outPose[i].quat = baseQuat;
				outPose[i].scale = baseScale;

				if (flags & CAnimDataBone::ANIMDATA_POS)
					outPose[i].pos = basePos + *animData.GetBonePosForFrame(static_cast<int>(i), frameIndex);

				if (flags & CAnimDataBone::ANIMDATA_ROT)
					outPose[i].quat = MultiplyQuaternion(baseQuat, *animData.GetBoneQuatForFrame(static_cast<int>(i), frameIndex));

				if (flags & CAnimDataBone::ANIMDATA_SCL)
				{
					const Vector* const deltaScale = animData.GetBoneScaleForFrame(static_cast<int>(i), frameIndex);
					outPose[i].scale = { baseScale.x * deltaScale->x, baseScale.y * deltaScale->y, baseScale.z * deltaScale->z };
				}

				if (i == 0) {
					Quaternion addRot(0.0f, 0.0f, 0.7071f, 0.7071f);
					outPose[i].quat = MultiplyQuaternion(outPose[i].quat, addRot);
				}
			}
			else
			{
				if (flags & CAnimDataBone::ANIMDATA_POS)
					outPose[i].pos = *animData.GetBonePosForFrame(static_cast<int>(i), frameIndex);

				if (flags & CAnimDataBone::ANIMDATA_ROT)
					outPose[i].quat = *animData.GetBoneQuatForFrame(static_cast<int>(i), frameIndex);

				if (flags & CAnimDataBone::ANIMDATA_SCL)
					outPose[i].scale = *animData.GetBoneScaleForFrame(static_cast<int>(i), frameIndex);
			}
		}

		return true;
	}

	void UpdateModelBoneMatrixInternal(CDXDrawData* const drawData, const ModelParsedData_t* const parsedData, const std::vector<PreviewBonePose_t>* const localPose)
	{
		ID3D11DeviceContext* const ctx = g_dxHandler->GetDeviceContext();

		D3D11_MAPPED_SUBRESOURCE resource;
		HRESULT hr = ctx->Map(
			drawData->boneMatrixBuffer, 0,
			D3D11_MAP_WRITE_DISCARD, 0,
			&resource
		);

		assert(SUCCEEDED(hr));

		if (FAILED(hr))
			return;

		XMMATRIX* boneArray = reinterpret_cast<XMMATRIX*>(resource.pData);
		std::vector<PreviewBonePose_t> defaultPose;
		const std::vector<PreviewBonePose_t>* poseData = localPose;
		if (!poseData)
		{
			defaultPose.resize(parsedData->bones.size());
			for (size_t i = 0; i < parsedData->bones.size(); ++i)
			{
				defaultPose[i].pos = parsedData->bones[i].pos;
				defaultPose[i].quat = parsedData->bones[i].quat;
				defaultPose[i].scale = parsedData->bones[i].scale;
			}
			poseData = &defaultPose;
		}

		std::vector<matrix3x4_t> worldMatrices;
		BuildWorldBoneMatrices(parsedData->bones, *poseData, worldMatrices);

		for (size_t i = 0; i < parsedData->bones.size(); ++i)
		{
			const XMMATRIX inverseBindMat = parsedData->boneInverseBindMatrices.at(i);
			const XMMATRIX skinningMatrix = XMMatrixMultiply(Matrix3x4ToXMMATRIX(worldMatrices[i]), inverseBindMat);
			boneArray[i] = ConvertSkinningMatrixToPreviewSpace(skinningMatrix);
		}

		ctx->Unmap(drawData->boneMatrixBuffer, 0);
	}

	void UpdatePreviewSkinnedMeshes_LOD(CDXDrawData* const drawData, const ModelParsedData_t* const parsedData, const std::vector<PreviewBonePose_t>& localPose, uint8_t lodLevel)
	{
		if (!drawData || !parsedData)
			return;

		ID3D11DeviceContext* const ctx = g_dxHandler->GetDeviceContext();
		if (!ctx)
			return;

		std::vector<matrix3x4_t> worldMatrices;
		BuildWorldBoneMatrices(parsedData->bones, localPose, worldMatrices);

		for (size_t meshIndex = 0; meshIndex < parsedData->lods.at(lodLevel).meshes.size(); ++meshIndex)
		{
			const ModelMeshData_t& mesh = parsedData->lods.at(lodLevel).meshes.at(meshIndex);
			DXMeshDrawData_t* const meshDrawData = &drawData->meshBuffers[meshIndex];
			if (!meshDrawData->vertexBuffer || mesh.meshVertexDataIndex == invalidNoodleIdx)
				continue;

			std::unique_ptr<char[]> parsedVertexDataBuf = parsedData->meshVertexData.getIdx(mesh.meshVertexDataIndex);
			const CMeshData* const parsedVertexData = reinterpret_cast<CMeshData*>(parsedVertexDataBuf.get());
			if (!parsedVertexData)
				continue;

			const Vertex_t* const srcVertices = parsedVertexData->GetVertices();
			const VertexWeight_t* const srcWeights = parsedVertexData->GetWeights();
			if (!srcVertices || !srcWeights)
				continue;

			if (mesh.vertCount == 0)
				continue;

			D3D11_MAPPED_SUBRESOURCE resource{};
			if (FAILED(ctx->Map(meshDrawData->vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource)))
				continue;

			if (!resource.pData)
			{
				ctx->Unmap(meshDrawData->vertexBuffer, 0);
				continue;
			}

			Vertex_t* const dstVertices = reinterpret_cast<Vertex_t*>(resource.pData);
			memcpy(dstVertices, srcVertices, sizeof(Vertex_t) * mesh.vertCount);

			for (size_t vertexIndex = 0; vertexIndex < mesh.vertCount; ++vertexIndex)
			{
				const Vertex_t& srcVertex = srcVertices[vertexIndex];
				Vector blendedPos{};
				float totalWeight = 0.0f;

				for (uint32_t weightIndex = 0; weightIndex < srcVertex.weightCount; ++weightIndex)
				{
					const VertexWeight_t& weight = srcWeights[srcVertex.weightIndex + weightIndex];
					if (weight.bone < 0 || static_cast<size_t>(weight.bone) >= worldMatrices.size() || weight.weight <= 0.0f)
						continue;

					const ModelBone_t& bone = parsedData->bones[weight.bone];
					if (!bone.poseToBone)
						continue;

					const Vector jointSpacePos = TransformPointByMatrix(*bone.poseToBone, srcVertex.position);
					const Vector animatedPos = TransformPointByMatrix(worldMatrices[weight.bone], jointSpacePos);

					blendedPos.x += animatedPos.x * weight.weight;
					blendedPos.y += animatedPos.y * weight.weight;
					blendedPos.z += animatedPos.z * weight.weight;
					totalWeight += weight.weight;
				}

				if (totalWeight <= 0.0f)
					continue;

				dstVertices[vertexIndex].position = blendedPos;
			}

			ctx->Unmap(meshDrawData->vertexBuffer, 0);
		}
	}

	void UpdatePreviewSkinnedMeshes(CDXDrawData* const drawData, const ModelParsedData_t* const parsedData, const std::vector<PreviewBonePose_t>& localPose)
	{
		UpdatePreviewSkinnedMeshes_LOD(drawData, parsedData, localPose, g_currentPreviewDrawData.activeLODLevel);
	}

	void AdvanceAndUploadPose(
		CDXDrawData* const drawData,
		ModelParsedData_t* const parsedData,
		const ModelParsedData_t* const animParsedData,
		const ModelSeq_t* const seq,
		const ModelAnim_t* const anim,
		const int frame,
		const uint8_t lodLevel,
		std::vector<PreviewBonePose_t>& outAnimPose,
		std::vector<PreviewBonePose_t>& outMeshPose,
		const std::vector<PreviewBonePose_t>* basePose = nullptr
	)
	{
		if (animParsedData && !animParsedData->bones.empty())
		{
			outAnimPose.resize(animParsedData->bones.size());
			for (size_t i = 0; i < animParsedData->bones.size(); ++i)
			{
				outAnimPose[i].pos = animParsedData->bones[i].pos;
				outAnimPose[i].quat = animParsedData->bones[i].quat;
				outAnimPose[i].scale = animParsedData->bones[i].scale;
			}

			if (seq && anim)
				EvaluateAnimationPose(seq, animParsedData, anim, frame, outAnimPose, basePose);
		}

		if (parsedData)
		{
			outMeshPose.resize(parsedData->bones.size());
			for (size_t i = 0; i < parsedData->bones.size(); ++i)
			{
				outMeshPose[i].pos = parsedData->bones[i].pos;
				outMeshPose[i].quat = parsedData->bones[i].quat;
				outMeshPose[i].scale = parsedData->bones[i].scale;
			}

			if (!outAnimPose.empty())
			{
				std::vector<int> remap;
				BuildBoneNameRemap(remap, parsedData->bones, animParsedData->bones);
				for (size_t i = 0; i < outMeshPose.size(); ++i)
				{
					const int animBone = remap[i];
					if (animBone >= 0)
						outMeshPose[i] = outAnimPose[animBone];
				}
			}

#if defined(HAS_BONED_MODELS)
			if (!drawData->boneMatrixBuffer)
				InitModelBoneMatrix(drawData, parsedData);

			if (drawData->boneMatrixBuffer)
			{
				UpdatePreviewSkinnedMeshes_LOD(drawData, parsedData, outMeshPose, lodLevel);
			}
#endif
		}
	}

	void DrawPreviewBones(CDXDrawData* const drawData, const ModelParsedData_t* const skeleton, const std::vector<PreviewBonePose_t>& localPose)
	{
		if (!skeleton || skeleton->bones.empty())
			return;

		std::vector<matrix3x4_t> worldMatrices;
		BuildWorldBoneMatrices(skeleton->bones, localPose, worldMatrices);

		for (size_t i = 0; i < skeleton->bones.size(); ++i)
		{
			const int parent = skeleton->bones[i].parent;
			if (parent < 0)
				continue;

			Vector child{};
			Vector parentVec{};
			MatrixPosition(worldMatrices[i], child);
			MatrixPosition(worldMatrices[parent], parentVec);

			child = ConvertPositionToPreviewSpace(child);
			parentVec = ConvertPositionToPreviewSpace(parentVec);

			if (!ShouldCullPreviewDebugLine(child, parentVec))
				drawData->DrawLine(child, parentVec, 0xFF00C8FF, false, 1.0f, -1.0f);
		}
	}

	void DrawPreviewBonesWithRootTransform(CDXDrawData* const drawData, const ModelParsedData_t* const skeleton, const std::vector<PreviewBonePose_t>& localPose, const matrix3x4_t& rootMatrix)
	{
		if (!skeleton || skeleton->bones.empty())
			return;

		std::vector<matrix3x4_t> worldMatrices;
		BuildWorldBoneMatrices(skeleton->bones, localPose, worldMatrices);

		for (size_t i = 0; i < skeleton->bones.size(); ++i)
		{
			const int parent = skeleton->bones[i].parent;
			if (parent < 0)
				continue;

			Vector child{};
			Vector parentVec{};
			MatrixPosition(worldMatrices[i], child);
			MatrixPosition(worldMatrices[parent], parentVec);

			child = TransformPointByMatrix(rootMatrix, child);
			parentVec = TransformPointByMatrix(rootMatrix, parentVec);

			child = ConvertPositionToPreviewSpace(child);
			parentVec = ConvertPositionToPreviewSpace(parentVec);

			if (!ShouldCullPreviewDebugLine(child, parentVec))
				drawData->DrawLine(child, parentVec, 0xFF00C8FF, false, 1.0f, -1.0f);
		}
	}

	void DrawPreviewAttachmentsWithRootTransform(CDXDrawData* const drawData, const ModelParsedData_t* const parsedData, const std::vector<matrix3x4_t>& boneWorldMatrices, const matrix3x4_t& rootMatrix)
	{
		if (!parsedData || parsedData->attachments.empty())
			return;

		for (const ModelAttachment_t& attachment : parsedData->attachments)
		{
			if (!attachment.localmatrix || attachment.localbone < 0 || static_cast<size_t>(attachment.localbone) >= boneWorldMatrices.size())
				continue;

			matrix3x4_t localWorldMatrix{};
			ConcatTransforms(boneWorldMatrices[attachment.localbone], *attachment.localmatrix, localWorldMatrix);

			matrix3x4_t propWorldMatrix{};
			ConcatTransforms(rootMatrix, localWorldMatrix, propWorldMatrix);

			Vector origin{};
			MatrixPosition(propWorldMatrix, origin);

			const Vector sourceX = TransformPointByMatrix(propWorldMatrix, { 2.0f, 0.0f, 0.0f });
			const Vector sourceY = TransformPointByMatrix(propWorldMatrix, { 0.0f, 2.0f, 0.0f });
			const Vector sourceZ = TransformPointByMatrix(propWorldMatrix, { 0.0f, 0.0f, 2.0f });

			const Vector previewOrigin = ConvertPositionToPreviewSpace(origin);
			const Vector previewX = ConvertPositionToPreviewSpace(sourceX);
			const Vector previewY = ConvertPositionToPreviewSpace(sourceY);
			const Vector previewZ = ConvertPositionToPreviewSpace(sourceZ);

			if (!ShouldCullPreviewDebugLine(previewOrigin, previewX))
				drawData->DrawLine(previewOrigin, previewX, 0xFFFF8000, false, 1.0f, -1.0f);
			if (!ShouldCullPreviewDebugLine(previewOrigin, previewY))
				drawData->DrawLine(previewOrigin, previewY, 0xFFFF8000, false, 1.0f, -1.0f);
			if (!ShouldCullPreviewDebugLine(previewOrigin, previewZ))
				drawData->DrawLine(previewOrigin, previewZ, 0xFFFF8000, false, 1.0f, -1.0f);
		}
	}

	bool ResolvePreviewSequenceState(const ModelPreviewInfo_t* const info, const ModelParsedData_t* const skeleton, const ModelSeq_t* const seqdesc, PreviewSequenceState_t& state)
	{
		state = {};

		if (!skeleton || !seqdesc)
			return false;

		const int animIndex = ResolvePreviewAnimationIndex(info, seqdesc);
		if (animIndex < 0)
			return false;

		state.sequence = seqdesc;
		state.animation = seqdesc->anims + animIndex;
		state.skeleton = skeleton;
		return true;
	}
}

void UpdateModelBoneMatrix(CDXDrawData* const drawData, const ModelParsedData_t* const parsedData)
{
	UpdateModelBoneMatrixInternal(drawData, parsedData, nullptr);
}

// Calculates a matrix that translates from model-space to joint-space.
// This is only calculated once when the model is first selected for preview, as it's completely useless for export
void CalculateBonesInverseBindMatrix(ModelParsedData_t* const parsedData)
{
	parsedData->boneInverseBindMatrices.resize(parsedData->bones.size());

	std::vector<matrix3x4_t> tempBoneMatrices(parsedData->bones.size());

	int i = 0;
	for (const ModelBone_t& bone : parsedData->bones)
	{
		CalcMatrixForBone_Unparented(bone, tempBoneMatrices[i]);

		// now handle parenting
		if (bone.parent != -1)
		{
			matrix3x4_t concatenated{};
			ConcatTransforms(tempBoneMatrices[bone.parent], tempBoneMatrices[i], concatenated);
			tempBoneMatrices[i] = concatenated;
		}

		XMVECTOR determinant;
		parsedData->boneInverseBindMatrices[i] = XMMatrixInverse(&determinant, Matrix3x4ToXMMATRIX(tempBoneMatrices[i]));

		assert(determinant.m128_f32[0] != 0 && determinant.m128_f32[1] != 0 && determinant.m128_f32[2] != 0);
		//XMMATRIX fuck = XMLoadFloat3x4((XMFLOAT3X4*)boneArray + i);
		++i;
	}
}

void InitModelBoneMatrix(CDXDrawData* const drawData, const ModelParsedData_t* const parsedData)
{
	ID3D11Device* const device = g_dxHandler->GetDevice();

	D3D11_BUFFER_DESC desc{};

	desc.ByteWidth = static_cast<UINT>(parsedData->bones.size()) * sizeof(matrix3x4_t);
	desc.ByteWidth = static_cast<UINT>(parsedData->bones.size()) * sizeof(XMMATRIX);
	desc.StructureByteStride = sizeof(XMMATRIX);

	// make sure this buffer can be updated every frame
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	// pixel is a const buffer
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	HRESULT hr = device->CreateBuffer(&desc, NULL, &drawData->boneMatrixBuffer);

#if defined(ASSERTS)
	assert(!FAILED(hr));
#else
	if (FAILED(hr))
		return;
#endif

	// SRV
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = static_cast<UINT>(parsedData->bones.size());

	hr = device->CreateShaderResourceView(drawData->boneMatrixBuffer, &srvDesc, &drawData->boneMatrixSRV);

#if defined(ASSERTS)
	assert(!FAILED(hr));
#else
	if (FAILED(hr))
		return;
#endif

	CalculateBonesInverseBindMatrix(const_cast<ModelParsedData_t*>(parsedData));

	// Initial update for the bone matrices
	UpdateModelBoneMatrix(drawData, parsedData);
}

void ModelPreviewInfo_t::ClearSpawnedProps()
{
	for (auto& prop : spawnedProps)
	{
		if (prop.drawData)
		{
			delete prop.drawData;
			prop.drawData = nullptr;
		}
	}
	spawnedProps.clear();
}

void ModelPreviewInfo_t::ClearExtraModelDrawDatas()
{
	for (CDXDrawData*& dd : extraModelDrawDatas)
	{
		delete dd;
		dd = nullptr;
	}
}

void ModelPreviewInfo_t::ClearExtraModelDrawData(int slot)
{
	if (slot >= 0 && slot < static_cast<int>(extraModelDrawDatas.size()))
	{
		delete extraModelDrawDatas[slot];
		extraModelDrawDatas[slot] = nullptr;
	}
}

ParsedCreatePropEvent_t ParseCreatePropOptions(const char* options)
{
	ParsedCreatePropEvent_t ev{};
	if (!options)
		return ev;

	std::istringstream iss(options);
	if (iss >> ev.hash >> ev.modelPath >> ev.attachment >> ev.seqPath)
	{
		ev.valid = true;
		iss >> ev.unk;
	}
	return ev;
}

std::string ParseBodygroupEventOptions(const char* options)
{
	if (!options)
		return "";

	std::string s(options);
	s.erase(0, s.find_first_not_of(" \t\r\n"));
	s.erase(s.find_last_not_of(" \t\r\n") + 1);
	return s;
}

void* PreviewParsedData(ModelPreviewInfo_t* const info, ModelParsedData_t* const meshParsedData, const ModelParsedData_t* const animParsedData, const ModelSeq_t* const previewSequence, char* const assetName, const uint64_t assetGUID, const uint64_t meshAssetGUID, const bool firstFrameForAsset)
{
	const bool hasMesh = meshParsedData && !meshParsedData->lods.empty();
	const uint64_t resolvedMeshGuid = hasMesh ? meshAssetGUID : assetGUID;
	const uint8_t activeLOD = hasMesh ? info->selectedLODLevel : 0u;
	const bool meshChanged = (info->activeMeshGuid != resolvedMeshGuid);

	if (meshChanged)
	{
		info->activeMeshGuid = resolvedMeshGuid;
		info->selectedBodypartIndex = 0u;
		info->lastSelectedBodypartIndex = 0u;
		info->selectedSkinIndex = 0u;
		info->lastSelectedSkinIndex = 0u;
		info->selectedLODLevel = 0u;

		if (hasMesh)
			info->bodygroupModelSelected.assign(meshParsedData->bodyParts.size(), 0ull);
		else
			info->bodygroupModelSelected.clear();
	}

	// [rika]: set up CDXDrawData
	g_currentPreviewDrawData.CheckForMonitorChange();

	if (resolvedMeshGuid != g_currentPreviewDrawData.guid || g_currentPreviewDrawData.GetDrawData() == nullptr || activeLOD != g_currentPreviewDrawData.activeLODLevel)
	{
		g_currentPreviewDrawData.FreeDrawData();

		CDXDrawData* const drawData = new CDXDrawData();

		if (hasMesh)
			drawData->meshBuffers.resize(meshParsedData->lods.at(activeLOD).meshes.size());

		drawData->modelName = assetName;
		drawData->dataType = CDXDrawData::DrawDataType_e::MODEL;

		if (hasMesh)
		{
			CreateBuffersForModelDrawData(meshParsedData, drawData, activeLOD);
			CreateBuffersForModelHitboxes(meshParsedData, drawData);
		}

		g_currentPreviewDrawData.UpdateAssetInfo(drawData, resolvedMeshGuid, activeLOD);
	}

	CDXDrawData* const drawData = g_currentPreviewDrawData.GetDrawData();
	if (!drawData)
		return nullptr;

	drawData->vertexShader = g_dxHandler->GetShaderManager()->LoadShaderFromString("shaders/model_vs", s_PreviewVertexShader, eShaderType::Vertex);
	drawData->pixelShader = g_dxHandler->GetShaderManager()->LoadShaderFromString("shaders/model_ps", s_PreviewPixelShader, eShaderType::Pixel);

	const ModelLODData_t* lodData = hasMesh ? &meshParsedData->lods.at(info->selectedLODLevel) : nullptr;

	if (hasMesh)
	{
		ImGui::SeparatorText("Model");

		ImGui::Text("LODs: %llu", meshParsedData->lods.size());

		if (info->minLODIndex != info->maxLODIndex)
			ImGui::SliderScalar("LOD Level", ImGuiDataType_U8, &info->selectedLODLevel, &info->minLODIndex, &info->maxLODIndex);

		if (!meshParsedData->skins.empty())
		{
			ImGui::TextUnformatted("Skins:");
			ImGui::SameLine();

			const char* skinLabel = meshParsedData->skins.at(info->selectedSkinIndex).name;

			if (ImGui::BeginCombo("##SKins", skinLabel, ImGuiComboFlags_NoArrowButton))
			{
				for (size_t i = 0; i < meshParsedData->skins.size(); i++)
				{
					const ModelSkinData_t& skin = meshParsedData->skins.at(i);
					const bool isSelected = info->selectedSkinIndex == i || (firstFrameForAsset && info->selectedSkinIndex == info->lastSelectedSkinIndex);

					if (ImGui::Selectable(skin.name, isSelected))
						info->selectedSkinIndex = i;

					if (isSelected) ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}
		}

		if (!meshParsedData->bodyParts.empty())
		{
			ImGui::TextUnformatted("Bodypart:");
			ImGui::SameLine();

			const char* bodypartLabel = meshParsedData->bodyParts.at(info->selectedBodypartIndex).GetNameCStr();

			if (ImGui::BeginCombo("##Bodypart", bodypartLabel, ImGuiComboFlags_NoArrowButton))
			{
				for (size_t i = 0; i < meshParsedData->bodyParts.size(); i++)
				{
					const ModelBodyPart_t& bodypart = meshParsedData->bodyParts.at(i);
					const bool isSelected = info->selectedBodypartIndex == i || (firstFrameForAsset && info->selectedBodypartIndex == info->lastSelectedBodypartIndex);

					if (ImGui::Selectable(bodypart.GetNameCStr(), isSelected))
						info->selectedBodypartIndex = i;

					if (isSelected) ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}

			if (info->selectedBodypartIndex >= meshParsedData->bodyParts.size())
				info->selectedBodypartIndex = 0;

			if (info->selectedSkinIndex >= meshParsedData->skins.size())
				info->selectedSkinIndex = 0;

			if (info->selectedLODLevel >= meshParsedData->lods.size())
				info->selectedLODLevel = 0;

			g_ExportSettings.previewedSkinIndex = static_cast<int>(info->selectedSkinIndex);

			if (meshParsedData->bodyParts.at(info->selectedBodypartIndex).numModels > 1)
			{
				const ModelBodyPart_t& bodypart = meshParsedData->bodyParts.at(info->selectedBodypartIndex);
				size_t& selectedModelIndex = info->bodygroupModelSelected.at(info->selectedBodypartIndex);

				ImGui::TextUnformatted("Model:");
				ImGui::SameLine();

				const char* modelLabel = lodData->models.at(bodypart.modelIndex + selectedModelIndex).name.c_str();

				if (ImGui::BeginCombo("##Model", modelLabel, ImGuiComboFlags_NoArrowButton))
				{
					for (int i = 0; i < bodypart.numModels; i++)
					{
						const bool isSelected = selectedModelIndex == i;
						const char* tmp = lodData->models.at(bodypart.modelIndex + i).name.c_str();
						if (ImGui::Selectable(tmp, isSelected))
							selectedModelIndex = static_cast<size_t>(i);

						if (isSelected) ImGui::SetItemDefaultFocus();
					}

					ImGui::EndCombo();
				}
			}
		}
	}

	PreviewSequenceState_t sequenceState{};
	const bool hasPreviewSequence = ResolvePreviewSequenceState(info, animParsedData, previewSequence, sequenceState);
	const bool hasCameraAttachment = meshParsedData && std::ranges::any_of(meshParsedData->attachments, [](const ModelAttachment_t& attachment)
		{
			return attachment.name && (!strcmp(attachment.name, "CAMERA") || !strcmp(attachment.name, "VDU"));
		});

	const bool isPreviewingSeqDelta = hasPreviewSequence && (sequenceState.sequence->flags & STUDIO_DELTA) != 0;

	if (!isPreviewingSeqDelta)
	{
		info->baseSequenceGuid = 0ull;
		info->autoSelectbaseSeq = false;
	}

	AnimSeqAsset* baseSeqAsset = nullptr;
	if (isPreviewingSeqDelta && animParsedData)
	{
		if (info->baseSequenceGuid == 0ull && !info->autoSelectbaseSeq)
		{
			for (int i = 0; i < animParsedData->numExternalSequences; ++i)
			{
				const uint64_t guid = animParsedData->externalSequences[i].guid;
				CPakAsset* const candidate = g_assetData.FindAssetByGUID<CPakAsset>(guid);
				if (!candidate || !candidate->hasExtraData())
					continue;
				AnimSeqAsset* const candidateSeq = candidate->extraData<AnimSeqAsset*>();
				if (candidateSeq && candidateSeq->seqdesc.szactivityname != nullptr && strcmp(candidateSeq->seqdesc.szactivityname, "ACT_VM_ADS_IN") == 0)
				{
					info->baseSequenceGuid = guid;
					break;
				}
			}
		}

		if (info->baseSequenceGuid != 0ull)
		{
			CPakAsset* const baseSeqPak = g_assetData.FindAssetByGUID<CPakAsset>(info->baseSequenceGuid);
			if (baseSeqPak && baseSeqPak->hasExtraData())
				baseSeqAsset = baseSeqPak->extraData<AnimSeqAsset*>();
		}
	}

	if (hasPreviewSequence) {
		ImGui::SeparatorText("Animation");
	}

	if (hasPreviewSequence && sequenceState.sequence->AnimCount() > 1)
	{
		info->selectedAnimationIndex = std::clamp(
			info->selectedAnimationIndex, 0, sequenceState.sequence->AnimCount() - 1);

		const ModelAnim_t* const selectedAnim = sequenceState.sequence->Anim(info->selectedAnimationIndex);
		const char* animLabel = (selectedAnim && selectedAnim->pszName() && selectedAnim->pszName()[0])
			? selectedAnim->pszName() : "<unnamed>";

		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::BeginCombo("##Anim", animLabel))
		{
			for (int i = 0; i < sequenceState.sequence->AnimCount(); ++i)
			{
				const ModelAnim_t* const anim = sequenceState.sequence->Anim(i);
				const char* name = (anim && anim->pszName() && anim->pszName()[0]) ? anim->pszName() : nullptr;
				const std::string label = name
					? std::format("{} [{}]", name, i)
					: std::format("<unnamed> [{}]", i);

				if (ImGui::Selectable(label.c_str(), info->selectedAnimationIndex == i))
				{
					info->selectedAnimationIndex = i;
					info->previewTime = 0.0f;
					info->previewFrame = 0;
					sequenceState.animation = anim;
				}
				if (info->selectedAnimationIndex == i)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	// Playback
	if (hasPreviewSequence)
	{
		const float fps = std::max(sequenceState.animation->fps, 1.0f);
		const float duration = std::max(sequenceState.animation->Duration(), 1.0f / fps);
		const int   maxFrame = std::max(sequenceState.animation->numframes - 1, 0);

		if (info->previewAnimationPlaying)
		{
			info->previewTime += ImGui::GetIO().DeltaTime;
			if (info->previewAnimationLoop)
				info->previewTime = std::fmod(info->previewTime, duration);
			else if (info->previewTime >= duration)
			{
				info->previewTime = duration;
				info->previewAnimationPlaying = false;
			}
		}
		info->previewFrame = std::clamp(static_cast<int>(std::round(info->previewTime * fps)), 0, maxFrame);

		if (ImGui::Button(info->previewAnimationPlaying ? " | | " : "  >  "))
			info->previewAnimationPlaying = !info->previewAnimationPlaying;

		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Loop").x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x * 2.0f - ImGui::GetStyle().ItemInnerSpacing.x);
		if (ImGui::SliderInt("##Frame", &info->previewFrame, 0, maxFrame, maxFrame > 0 ? "Frame %d" : "Frame 0"))
			info->previewTime = info->previewFrame / fps;

		ImGui::SameLine();
		ImGui::Checkbox("Loop", &info->previewAnimationLoop);
	}
	else
	{
		info->previewTime = 0.0f;
		info->previewFrame = 0;
	}

	if (hasPreviewSequence)
	{
		if (info->eventLastFiredFrame.size() != previewSequence->numevents)
		{
			info->eventLastFiredFrame.assign(previewSequence->numevents, -1);
			info->ClearSpawnedProps();
		}

		if (info->previewFrame == 0)
		{
			info->eventLastFiredFrame.assign(previewSequence->numevents, -1);
		}

		for (int evIdx = 0; evIdx < previewSequence->numevents; ++evIdx)
		{
			const ModelEvent_t& ev = previewSequence->events[evIdx];
			int numFrames = sequenceState.animation->numframes;
			int eventFrame = static_cast<int>(std::floor(ev.cycle * (numFrames - 1)));

			if (info->previewFrame >= eventFrame && info->eventLastFiredFrame[evIdx] != eventFrame)
			{
				info->eventLastFiredFrame[evIdx] = eventFrame;

				if (ev.name)
				{
					if (strcmp(ev.name, "AE_CL_CREATE_PROP") == 0)
					{
						DispatchCreatePropEvent(info, ev);
					}
					else if (strcmp(ev.name, "AE_ENABLE_BODYGROUP") == 0)
					{
						DispatchBodygroupEvent(info, ev, meshParsedData, true);
					}
					else if (strcmp(ev.name, "AE_DISABLE_BODYGROUP") == 0)
					{
						DispatchBodygroupEvent(info, ev, meshParsedData, false);
					}
				}
			}
		}
	}
	else
	{
		info->eventLastFiredFrame.clear();
		info->ClearSpawnedProps();
	}

	if (isPreviewingSeqDelta && animParsedData)
	{
		ImGui::SeparatorText("Delta");

		std::string baseLabelStr = "<none>";
		if (baseSeqAsset && baseSeqAsset->seqdesc.szlabel)
		{
			baseLabelStr = baseSeqAsset->seqdesc.szlabel;
			if (baseSeqAsset->seqdesc.flags & STUDIO_DELTA)
				baseLabelStr += " [D]";
		}

		if (ImGui::BeginCombo("Base Sequence", baseLabelStr.c_str()))
		{
			const bool noneSelected = (info->baseSequenceGuid == 0ull);
			if (ImGui::Selectable("<none>", noneSelected)) {
				info->baseSequenceGuid = 0ull;
				info->baseAnimationIndex = 0;
				info->autoSelectbaseSeq = true;
				baseSeqAsset = nullptr;
			}

			if (noneSelected)
				ImGui::SetItemDefaultFocus();

			for (int i = 0; i < animParsedData->numExternalSequences; ++i)
			{
				const uint64_t guid = animParsedData->externalSequences[i].guid;
				CPakAsset* const candidate = g_assetData.FindAssetByGUID<CPakAsset>(guid);
				if (!candidate || !candidate->hasExtraData())
					continue;

				const AnimSeqAsset* const candidateSeq = candidate->extraData<AnimSeqAsset*>();
				if (!candidateSeq)
					continue;

				const char* seqLabelRaw = candidateSeq->seqdesc.szlabel ? candidateSeq->seqdesc.szlabel : candidate->GetAssetName().c_str();
				const bool isDeltaSeq = (candidateSeq->seqdesc.flags & STUDIO_DELTA) != 0;
				const std::string seqLabel = isDeltaSeq ? std::string(seqLabelRaw) + " [D]" : seqLabelRaw;
				const bool isSelected = (info->baseSequenceGuid == guid);

				if (ImGui::Selectable(seqLabel.c_str(), isSelected))
				{
					info->baseSequenceGuid = guid;
					info->baseAnimationIndex = 0;
					CPakAsset* const newPak = g_assetData.FindAssetByGUID<CPakAsset>(guid);
					baseSeqAsset = (newPak && newPak->hasExtraData()) ? newPak->extraData<AnimSeqAsset*>() : nullptr;
				}

				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}

		if (baseSeqAsset && baseSeqAsset->seqdesc.AnimCount() > 1)
		{
			info->baseAnimationIndex = std::clamp(info->baseAnimationIndex, 0, baseSeqAsset->seqdesc.AnimCount() - 1);

			const ModelAnim_t* const selectedBaseAnim = baseSeqAsset->seqdesc.Anim(info->baseAnimationIndex);
			const char* baseAnimLabel = (selectedBaseAnim && selectedBaseAnim->pszName() && selectedBaseAnim->pszName()[0])
				? selectedBaseAnim->pszName() : "<unnamed>";

			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::BeginCombo("##BaseAnim", baseAnimLabel))
			{
				for (int i = 0; i < baseSeqAsset->seqdesc.AnimCount(); ++i)
				{
					const ModelAnim_t* const anim = baseSeqAsset->seqdesc.Anim(i);
					const char* name = (anim && anim->pszName() && anim->pszName()[0]) ? anim->pszName() : nullptr;
					const std::string label = name
						? std::format("{} [{}]", name, i)
						: std::format("<unnamed> [{}]", i);

					if (ImGui::Selectable(label.c_str(), info->baseAnimationIndex == i))
						info->baseAnimationIndex = i;

					if (info->baseAnimationIndex == i)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}
	}

	ImGui::SeparatorText("Misc");

	if (!hasCameraAttachment) ImGui::BeginDisabled();
	if (ImGui::Button(info->attachCameraToCameraAttachment ? "Detach Camera" : "Attach Camera"))
		info->attachCameraToCameraAttachment = !info->attachCameraToCameraAttachment;

	if (!hasCameraAttachment)
	{
		ImGui::EndDisabled();
		info->attachCameraToCameraAttachment = false;
	}

	ImGui::SameLine();
	ImGui::Checkbox("Props", &info->showProps);
	ImGui::SameLine();
	ImGui::Checkbox("Bones", &info->showBones);
	ImGui::SameLine();
	ImGui::Checkbox("Attachments", &info->showAttachments);

	if (hasPreviewSequence)
	{
		ImGui::Spacing();
		if (ImGui::TreeNodeEx("Sequence Details", ImGuiTreeNodeFlags_SpanAvailWidth))
		{
			PreviewSeqDesc(sequenceState.sequence);
			ImGui::TreePop();
		}

		if (previewSequence->numevents > 0 && info->eventLastFiredFrame.size() == static_cast<size_t>(previewSequence->numevents))
		{
			ImGui::Spacing();
			if (ImGui::TreeNodeEx("Sequence Events", ImGuiTreeNodeFlags_SpanAvailWidth))
			{
				for (int i = 0; i < previewSequence->numevents; ++i)
				{
					const ModelEvent_t& ev = previewSequence->events[i];
					bool fired = info->eventLastFiredFrame[i] >= 0;
					if (fired)
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));

					std::string displayStr = std::format("[{:.3f}] {} {}", ev.cycle, ev.name ? ev.name : "unnamed", ev.options ? ev.options : "");
					ImGui::TextUnformatted(displayStr.c_str());

					if (fired)
						ImGui::PopStyleColor();
				}
				ImGui::TreePop();
			}
		}
	}

	if (!info->spawnedProps.empty())
	{
		ImGui::Spacing();
		if (ImGui::TreeNodeEx("Spawned Props", ImGuiTreeNodeFlags_SpanAvailWidth))
		{
			for (const auto& prop : info->spawnedProps)
			{
				std::string displayStr = "[" + prop.attachmentName + "] " + prop.seqPath + " @ " + prop.modelPath;
				if (prop.cachedMeshParsedData == nullptr)
					displayStr += " (resolving...)";

				ImGui::TextUnformatted(displayStr.c_str());
			}
			ImGui::TreePop();
		}
	}

	std::vector<PreviewBonePose_t> animPose;
	std::vector<PreviewBonePose_t> meshPose;
	std::vector<PreviewBonePose_t> basePoseForDelta;

	const std::vector<PreviewBonePose_t>* pBasePoseForDelta = nullptr;
	if (isPreviewingSeqDelta && baseSeqAsset && baseSeqAsset->animationParsed && animParsedData)
	{
		const ModelSeq_t* const baseSeq = &baseSeqAsset->seqdesc;
		if (baseSeq->AnimCount() > 0)
		{
			const int baseAnimIdx = std::clamp(info->baseAnimationIndex, 0, baseSeq->AnimCount() - 1);
			const ModelAnim_t* const baseAnim = baseSeq->Anim(baseAnimIdx);
			if (EvaluateAnimationPose(baseSeq, animParsedData, baseAnim, 0, basePoseForDelta))
				pBasePoseForDelta = &basePoseForDelta;
		}
	}

	AdvanceAndUploadPose(drawData, meshParsedData, animParsedData,
		hasPreviewSequence ? sequenceState.sequence : nullptr,
		hasPreviewSequence ? sequenceState.animation : nullptr,
		info->previewFrame,
		g_currentPreviewDrawData.activeLODLevel,
		animPose,
		meshPose,
		pBasePoseForDelta);

	std::vector<matrix3x4_t> meshWorldMatrices;
	std::vector<PreviewAttachmentWorld_t> previewAttachments;
	if (meshParsedData && hasMesh)
	{
		BuildWorldBoneMatrices(meshParsedData->bones, meshPose, meshWorldMatrices);
		BuildAttachmentWorldTransforms(meshParsedData, meshWorldMatrices, previewAttachments);
	}

	drawData->childDrawDatas.clear();

	// Extra model slots
	for (int extraIdx = 0; extraIdx < 2; ++extraIdx)
	{
		const uint64_t extraGuid = info->extraModelGuids[extraIdx];
		if (extraGuid == 0)
			continue;

		CPakAsset* const extraModelPak = g_assetData.FindAssetByGUID<CPakAsset>(extraGuid);
		if (!extraModelPak)
			continue;
		ModelAsset* const extraModelAsset = extraModelPak->extraData<ModelAsset*>();
		if (!extraModelAsset)
			continue;
		ModelParsedData_t* const extraParsedData = extraModelAsset->GetParsedData();
		if (!extraParsedData || extraParsedData->lods.empty())
			continue;

		const uint8_t extraLOD = std::min(activeLOD, static_cast<uint8_t>(extraParsedData->lods.size() - 1));

		CDXDrawData*& extraDrawData = info->extraModelDrawDatas[extraIdx];
		if (extraDrawData == nullptr)
		{
			extraDrawData = new CDXDrawData();
			extraDrawData->meshBuffers.resize(extraParsedData->lods.at(extraLOD).meshes.size());
			extraDrawData->modelName = const_cast<char*>(extraModelPak->GetAssetName().c_str());
			extraDrawData->dataType = CDXDrawData::DrawDataType_e::MODEL;
			CreateBuffersForModelDrawData(extraParsedData, extraDrawData, extraLOD);
			CreateBuffersForModelHitboxes(extraParsedData, extraDrawData);
		}

		extraDrawData->vertexShader = drawData->vertexShader;
		extraDrawData->pixelShader = drawData->pixelShader;

		const ModelLODData_t& extraLodData = extraParsedData->lods.at(extraLOD);
		for (size_t meshIndex = 0; meshIndex < extraLodData.meshes.size(); ++meshIndex)
		{
			const ModelMeshData_t& mesh = extraLodData.meshes.at(meshIndex);
			DXMeshDrawData_t* const mdd = &extraDrawData->meshBuffers[meshIndex];

			mdd->indexFormat = DXGI_FORMAT_R16_UINT;
			mdd->wireframe = false;
			mdd->visible = extraParsedData->bodyParts[mesh.bodyPartIndex].IsPreviewEnabled();

			const ModelBodyPart_t& bodypart = extraParsedData->bodyParts[mesh.bodyPartIndex];
			if (bodypart.modelIndex >= 0 && bodypart.modelIndex < static_cast<int>(extraLodData.models.size()))
			{
				const ModelModelData_t& model = extraLodData.models.at(bodypart.modelIndex);
				mdd->visible = (meshIndex >= model.meshIndex && meshIndex < model.meshIndex + model.meshCount);
			}

			mdd->pixelShader = extraDrawData->pixelShader->Get<ID3D11PixelShader>();
			mdd->vertexShader = extraDrawData->vertexShader->Get<ID3D11VertexShader>();
			mdd->inputLayout = extraDrawData->vertexShader->GetInputLayout();
			mdd->hasGameShaders = false;

			if (extraParsedData->skins.empty())
				continue;

			const ModelSkinData_t* skinData = &extraParsedData->skins.at(0);
			CPakAsset* const matlAsset = extraParsedData->materials.at(skinData->indices[mesh.materialId]).asset;
			if (!matlAsset)
				continue;

			const MaterialAsset* const matl = reinterpret_cast<MaterialAsset*>(matlAsset->extraData());
			if (mdd->textures.empty() && matl)
			{
				mdd->textures.clear();
				for (auto& texEntry : matl->txtrAssets)
				{
					if (texEntry.asset)
					{
						TextureAsset* txtr = reinterpret_cast<TextureAsset*>(texEntry.asset->extraData());
						for (auto& mip : txtr->mipArray | std::views::reverse)
						{
							if (mip.isLoaded)
							{
								const std::shared_ptr<CTexture> highestTextureMip = CreateTextureFromMip(texEntry.asset, &mip, s_PakToDxgiFormat[txtr->imgFormat]);
								mdd->textures.push_back({ texEntry.index, highestTextureMip });
								break;
							}
						}
					}
				}
			}
		}

		std::vector<PreviewBonePose_t> tempAnimPose;
		AdvanceAndUploadPose(
			extraDrawData,
			extraParsedData,
			animParsedData,
			hasPreviewSequence ? sequenceState.sequence : nullptr,
			hasPreviewSequence ? sequenceState.animation : nullptr,
			info->previewFrame,
			extraLOD,
			tempAnimPose,
			info->extraModelLastMeshPose[extraIdx],
			pBasePoseForDelta
		);

		for (auto& rsrc : drawData->pixelShaderResources)
			extraDrawData->SetPSResource(rsrc.first, rsrc.second);

		drawData->childDrawDatas.push_back(extraDrawData);
	}

	for (auto& prop : info->spawnedProps)
	{
		if (!info->showProps)
			continue;

		if (prop.cachedMeshParsedData == nullptr)
		{
			CPakAsset* const modelPak = g_assetData.FindAssetByGUID<CPakAsset>(prop.modelGuid);
			if (!modelPak)
				continue;
			ModelAsset* const propModelAsset = modelPak->extraData<ModelAsset*>();
			if (!propModelAsset)
				continue;
			prop.cachedMeshParsedData = propModelAsset->GetParsedData();

			if (propModelAsset->numAnimRigs > 0)
			{
				uint64_t rigGuid = propModelAsset->animRigs[0].guid;
				CPakAsset* const rigPak = g_assetData.FindAssetByGUID<CPakAsset>(rigGuid);
				if (rigPak)
				{
					AnimRigAsset* const rigAsset = rigPak->extraData<AnimRigAsset*>();
					if (rigAsset)
						prop.cachedAnimParsedData = rigAsset->GetParsedData();
				}
			}

			if (!prop.cachedAnimParsedData)
				prop.cachedAnimParsedData = prop.cachedMeshParsedData;

			CPakAsset* const seqPak = g_assetData.FindAssetByGUID<CPakAsset>(prop.seqGuid);
			if (seqPak)
			{
				AnimSeqAsset* const seqAsset = seqPak->extraData<AnimSeqAsset*>();
				if (seqAsset && seqAsset->animationParsed)
					prop.cachedSeq = &seqAsset->seqdesc;
			}
		}

		if (prop.cachedMeshParsedData == nullptr || prop.cachedAnimParsedData == nullptr || prop.cachedSeq == nullptr)
			continue;

		const PreviewAttachmentWorld_t* attachWorld = FindPreviewAttachmentByName(previewAttachments, prop.attachmentName.c_str());
		if (!attachWorld)
			continue;

		if (!sequenceState.animation || sequenceState.animation->fps <= 0.0f)
			continue;

		float mainFps = sequenceState.animation->fps;
		float mainDuration = sequenceState.animation->Duration();
		int mainFrame = info->previewFrame;
		float mainTime = mainFrame / mainFps;

		float propTime = std::max(mainTime - prop.spawnCycle * mainDuration, 0.0f);

		if (prop.cachedSeq->AnimCount() == 0)
			continue;

		const ModelAnim_t& propAnim = prop.cachedSeq->anims[0];
		float propFps = std::max(propAnim.fps, 1.0f);
		int propMaxFrame = std::max(propAnim.numframes - 1, 0);
		float propDuration = std::max(propAnim.Duration(), 1.0f / propFps);

		if (prop.looping)
			propTime = std::fmod(propTime, propDuration);
		else
			propTime = std::min(propTime, propDuration);

		int propFrame = std::clamp(static_cast<int>(std::round(propTime * propFps)), 0, propMaxFrame);

		if (mainTime < prop.spawnCycle * mainDuration)
			continue;

		if (prop.drawData == nullptr)
		{
			prop.drawData = new CDXDrawData();
			prop.drawData->meshBuffers.resize(prop.cachedMeshParsedData->lods.at(0).meshes.size());

			CPakAsset* const modelPak = g_assetData.FindAssetByGUID<CPakAsset>(prop.modelGuid);
			if (modelPak)
				prop.drawData->modelName = const_cast<char*>(modelPak->GetAssetName().c_str());

			prop.drawData->dataType = CDXDrawData::DrawDataType_e::MODEL;
			CreateBuffersForModelDrawData(prop.cachedMeshParsedData, prop.drawData, 0);
			CreateBuffersForModelHitboxes(prop.cachedMeshParsedData, prop.drawData);
		}

		prop.drawData->vertexShader = drawData->vertexShader;
		prop.drawData->pixelShader = drawData->pixelShader;

		for (size_t meshIndex = 0; meshIndex < prop.cachedMeshParsedData->lods.at(0).meshes.size(); ++meshIndex)
		{
			const ModelMeshData_t& mesh = prop.cachedMeshParsedData->lods.at(0).meshes.at(meshIndex);
			DXMeshDrawData_t* const meshDrawData = &prop.drawData->meshBuffers[meshIndex];

			meshDrawData->indexFormat = DXGI_FORMAT_R16_UINT;
			meshDrawData->wireframe = false;
			meshDrawData->visible = prop.cachedMeshParsedData->bodyParts[mesh.bodyPartIndex].IsPreviewEnabled();

			const ModelBodyPart_t& bodypart = prop.cachedMeshParsedData->bodyParts[mesh.bodyPartIndex];
			if (bodypart.modelIndex >= 0 && bodypart.modelIndex < prop.cachedMeshParsedData->lods.at(0).models.size())
			{
				const ModelModelData_t& model = prop.cachedMeshParsedData->lods.at(0).models.at(bodypart.modelIndex);
				if (meshIndex >= model.meshIndex && meshIndex < model.meshIndex + model.meshCount)
					meshDrawData->visible = true;
				else
					meshDrawData->visible = false;
			}

			meshDrawData->pixelShader = prop.drawData->pixelShader->Get<ID3D11PixelShader>();
			meshDrawData->vertexShader = prop.drawData->vertexShader->Get<ID3D11VertexShader>();
			meshDrawData->inputLayout = prop.drawData->vertexShader->GetInputLayout();
			meshDrawData->hasGameShaders = false;

			if (prop.cachedMeshParsedData->skins.empty())
				continue;

			const ModelSkinData_t* skinData = &prop.cachedMeshParsedData->skins.at(0);
			CPakAsset* const matlAsset = prop.cachedMeshParsedData->materials.at(skinData->indices[mesh.materialId]).asset;
			if (!matlAsset)
				continue;

			const MaterialAsset* const matl = reinterpret_cast<MaterialAsset*>(matlAsset->extraData());
			if (meshDrawData->textures.size() == 0 && matl)
			{
				meshDrawData->textures.clear();
				for (auto& texEntry : matl->txtrAssets)
				{
					if (texEntry.asset)
					{
						TextureAsset* txtr = reinterpret_cast<TextureAsset*>(texEntry.asset->extraData());
						for (auto& mip : txtr->mipArray | std::views::reverse)
						{
							if (mip.isLoaded)
							{
								const std::shared_ptr<CTexture> highestTextureMip = CreateTextureFromMip(texEntry.asset, &mip, s_PakToDxgiFormat[txtr->imgFormat]);
								meshDrawData->textures.push_back({ texEntry.index, highestTextureMip });
								break;
							}
						}
					}
				}
			}
		}

		std::vector<PreviewBonePose_t> tempAnimPose;
		AdvanceAndUploadPose(
			prop.drawData,
			prop.cachedMeshParsedData,
			prop.cachedAnimParsedData,
			prop.cachedSeq,
			&propAnim,
			propFrame,
			0,
			tempAnimPose,
			prop.lastPose
		);

		prop.drawData->useWorldTransform = true;

		matrix3x4_t rotationMatrix = matrix3x4_t::Identity();
		rotationMatrix[0][0] = 0.0f;  rotationMatrix[0][1] = -1.0f; rotationMatrix[0][2] = 0.0f;
		rotationMatrix[1][0] = 1.0f;   rotationMatrix[1][1] = 0.0f;  rotationMatrix[1][2] = 0.0f;
		rotationMatrix[2][0] = 0.0f;   rotationMatrix[2][1] = 0.0f;  rotationMatrix[2][2] = 1.0f;

		matrix3x4_t propWorldMatrix;
		ConcatTransforms(attachWorld->worldMatrix, rotationMatrix, propWorldMatrix);

		prop.drawData->worldTransform = propWorldMatrix;

		for (auto& rsrc : drawData->pixelShaderResources)
			prop.drawData->SetPSResource(rsrc.first, rsrc.second);

		drawData->childDrawDatas.push_back(prop.drawData);
	}

	ClearDebugPrimitives(drawData);
	if (info->showBones && !animPose.empty())
		DrawPreviewBones(drawData, animParsedData, animPose);
	if (info->showAttachments && !previewAttachments.empty())
		DrawPreviewAttachments(drawData, previewAttachments);

	for (auto& prop : info->spawnedProps)
	{
		if (!info->showProps)
			continue;

		if (prop.cachedMeshParsedData == nullptr || prop.lastPose.empty())
			continue;

		const PreviewAttachmentWorld_t* attachWorld = FindPreviewAttachmentByName(previewAttachments, prop.attachmentName.c_str());
		if (!attachWorld)
			continue;

		// Rotate 90 degrees to the left (yaw) around local Z-axis (up) in Z-up space
		matrix3x4_t rotationMatrix = matrix3x4_t::Identity();
		rotationMatrix[0][0] = 0.0f;  rotationMatrix[0][1] = -1.0f; rotationMatrix[0][2] = 0.0f;
		rotationMatrix[1][0] = 1.0f;   rotationMatrix[1][1] = 0.0f;  rotationMatrix[1][2] = 0.0f;
		rotationMatrix[2][0] = 0.0f;   rotationMatrix[2][1] = 0.0f;  rotationMatrix[2][2] = 1.0f;

		matrix3x4_t propWorldMatrix;
		ConcatTransforms(attachWorld->worldMatrix, rotationMatrix, propWorldMatrix);

		if (info->showBones && prop.cachedAnimParsedData)
		{
			DrawPreviewBonesWithRootTransform(
				drawData,
				prop.cachedAnimParsedData,
				prop.lastPose,
				propWorldMatrix
			);
		}

		if (info->showAttachments)
		{
			std::vector<PreviewBonePose_t> propMeshPose;
			propMeshPose.resize(prop.cachedMeshParsedData->bones.size());
			for (size_t i = 0; i < prop.cachedMeshParsedData->bones.size(); ++i)
			{
				propMeshPose[i].pos = prop.cachedMeshParsedData->bones[i].pos;
				propMeshPose[i].quat = prop.cachedMeshParsedData->bones[i].quat;
				propMeshPose[i].scale = prop.cachedMeshParsedData->bones[i].scale;
			}

			if (prop.cachedAnimParsedData != prop.cachedMeshParsedData)
			{
				std::vector<int> remap;
				BuildBoneNameRemap(remap, prop.cachedMeshParsedData->bones, prop.cachedAnimParsedData->bones);
				for (size_t i = 0; i < propMeshPose.size(); ++i)
				{
					const int animBone = remap[i];
					if (animBone >= 0)
						propMeshPose[i] = prop.lastPose[animBone];
				}
			}
			else
			{
				for (size_t i = 0; i < propMeshPose.size(); ++i)
					propMeshPose[i] = prop.lastPose[i];
			}

			std::vector<matrix3x4_t> propMeshWorldMatrices;
			BuildWorldBoneMatrices(prop.cachedMeshParsedData->bones, propMeshPose, propMeshWorldMatrices);

			DrawPreviewAttachmentsWithRootTransform(
				drawData,
				prop.cachedMeshParsedData,
				propMeshWorldMatrices,
				propWorldMatrix
			);
		}
	}

	for (int extraIdx = 0; extraIdx < 2; ++extraIdx)
	{
		if (info->extraModelGuids[extraIdx] == 0 || info->extraModelDrawDatas[extraIdx] == nullptr)
			continue;

		CPakAsset* const extraModelPak = g_assetData.FindAssetByGUID<CPakAsset>(info->extraModelGuids[extraIdx]);
		if (!extraModelPak)
			continue;
		ModelAsset* const extraModelAsset = extraModelPak->extraData<ModelAsset*>();
		if (!extraModelAsset)
			continue;
		ModelParsedData_t* const extraParsedData = extraModelAsset->GetParsedData();
		if (!extraParsedData)
			continue;

		const std::vector<PreviewBonePose_t>& extraMeshPose = info->extraModelLastMeshPose[extraIdx];
		if (extraMeshPose.empty())
			continue;

		if (info->showBones && animParsedData)
			DrawPreviewBones(drawData, extraParsedData, extraMeshPose);

		if (info->showAttachments)
		{
			std::vector<matrix3x4_t> extraMeshWorldMatrices;
			BuildWorldBoneMatrices(extraParsedData->bones, extraMeshPose, extraMeshWorldMatrices);

			std::vector<PreviewAttachmentWorld_t> extraAttachments;
			BuildAttachmentWorldTransforms(extraParsedData, extraMeshWorldMatrices, extraAttachments);
			DrawPreviewAttachments(drawData, extraAttachments);
		}
	}

	if (info->attachCameraToCameraAttachment)
	{
		if (const PreviewAttachmentWorld_t* const cameraAttachment = FindPreviewAttachmentByName(previewAttachments, "CAMERA"))
			ApplyPreviewCameraAttachment(*cameraAttachment);
		else if (const PreviewAttachmentWorld_t* const vduAttachment = FindPreviewAttachmentByName(previewAttachments, "VDU"))
			ApplyPreviewCameraAttachment(*vduAttachment);
		else
		{
			info->attachCameraToCameraAttachment = false;
			DisableAttachedPreviewCamera();
		}
	}
	else
		DisableAttachedPreviewCamera();

	// Load these first so we don't have to look them up for every mesh.
#if defined(ADVANCED_MODEL_PREVIEW)
	const CShader* const vertexShader = g_dxHandler->GetShaderManager()->LoadShader("C:/p4/rtech_utils_imgui/src/shaders/amp_vs", eShaderType::Vertex);
#else
	const CShader* const vertexShader = g_dxHandler->GetShaderManager()->LoadShaderFromString("shaders/model_vs", s_PreviewVertexShader, eShaderType::Vertex);
#endif
	const CShader* const pixelShader = g_dxHandler->GetShaderManager()->LoadShaderFromString("shaders/model_ps", s_PreviewPixelShader, eShaderType::Pixel);

	if (hasMesh)
	{
		const ModelSkinData_t* skinData = meshParsedData->skins.empty() ? nullptr : &meshParsedData->skins.at(info->selectedSkinIndex);
		for (size_t i = 0; i < lodData->meshes.size(); ++i)
		{
			const ModelMeshData_t& mesh = lodData->meshes.at(i);
			DXMeshDrawData_t* const meshDrawData = &drawData->meshBuffers[i];

			meshDrawData->indexFormat = DXGI_FORMAT_R16_UINT;
			meshDrawData->wireframe = false;
			drawData->meshBuffers[i].visible = meshParsedData->bodyParts[mesh.bodyPartIndex].IsPreviewEnabled();

			const ModelBodyPart_t& bodypart = meshParsedData->bodyParts[mesh.bodyPartIndex];
			const ModelModelData_t& model = lodData->models.at(bodypart.modelIndex + info->bodygroupModelSelected.at(mesh.bodyPartIndex));

			if (bodypart.IsPreviewEnabled() && i >= model.meshIndex && i < model.meshIndex + model.meshCount)
				drawData->meshBuffers[i].visible = true;
			else
				drawData->meshBuffers[i].visible = false;

			meshDrawData->pixelShader = pixelShader->Get<ID3D11PixelShader>();
			meshDrawData->vertexShader = vertexShader->Get<ID3D11VertexShader>();
			meshDrawData->inputLayout = vertexShader->GetInputLayout();
			meshDrawData->hasGameShaders = false;

			if (!skinData)
				continue;

			CPakAsset* const matlAsset = meshParsedData->materials.at(skinData->indices[mesh.materialId]).asset;
			if (!matlAsset)
				continue;

			const MaterialAsset* const matl = reinterpret_cast<MaterialAsset*>(matlAsset->extraData());

#if defined(ADVANCED_MODEL_PREVIEW)
			if (matl->shaderSetAsset)
			{
				ShaderSetAsset* const shaderSet = reinterpret_cast<ShaderSetAsset*>(matl->shaderSetAsset->extraData());
				if (shaderSet->vertexShaderAsset && shaderSet->pixelShaderAsset)
				{
					ShaderAsset* const shadersetPS = reinterpret_cast<ShaderAsset*>(shaderSet->pixelShaderAsset->extraData());
					meshDrawData->pixelShader = shadersetPS->pixelShader;
					meshDrawData->hasGameShaders = true;
				}
			}
#endif

			if ((meshDrawData->textures.size() == 0 || info->lastSelectedSkinIndex != info->selectedSkinIndex) && matl)
			{
				meshDrawData->textures.clear();
				for (auto& texEntry : matl->txtrAssets)
				{
					if (texEntry.asset)
					{
						TextureAsset* txtr = reinterpret_cast<TextureAsset*>(texEntry.asset->extraData());
						for (auto& mip : txtr->mipArray | std::views::reverse)
						{
							if (mip.isLoaded)
							{
								const std::shared_ptr<CTexture> highestTextureMip = CreateTextureFromMip(texEntry.asset, &mip, s_PakToDxgiFormat[txtr->imgFormat]);
								meshDrawData->textures.push_back({ texEntry.index, highestTextureMip });
								break;
							}
						}
					}
				}
			}
		}
	}

	Preview_MapTransformsBuffer(drawData);
	Preview_MapModelInstanceBuffer(drawData);

	for (int extraIdx = 0; extraIdx < 2; ++extraIdx)
	{
		CDXDrawData* extraDrawData = info->extraModelDrawDatas[extraIdx];
		if (extraDrawData){
			Preview_MapTransformsBuffer(extraDrawData);
			Preview_MapModelInstanceBuffer(extraDrawData);
		}
	}

	for (auto& prop : info->spawnedProps)
	{
		if (prop.drawData)
		{
			Preview_MapTransformsBuffer(prop.drawData);
			Preview_MapModelInstanceBuffer(prop.drawData);
		}
	}

	if (info->lastSelectedBodypartIndex != info->selectedBodypartIndex) UNLIKELY
		info->lastSelectedBodypartIndex = info->selectedBodypartIndex;

	if (info->lastSelectedSkinIndex != info->selectedSkinIndex) UNLIKELY
		info->lastSelectedSkinIndex = info->selectedSkinIndex;

	return drawData;
}

void PreviewAnimDesc(const ModelAnim_t* const animdesc, const int index)
{
	if (ImGui::TreeNodeEx(std::to_string(index).c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
	{
		ImGui::Text("Name: %s", animdesc->name);
		ImGui::Text("Flags: 0x%x", animdesc->flags);

		ImGui::Text("Frame Rate: %f", animdesc->fps);
		ImGui::Text("Frame Count: %i", animdesc->numframes);
		ImGui::Text("Duration: %.3f seconds", animdesc->Duration());

		ImGui::TreePop();
	}
}

void PreviewSeqDesc(const ModelSeq_t* const seqdesc)
{
	ImGui::Text("Label: %s", seqdesc->szlabel);
	ImGui::Text("Activity: %s", seqdesc->szactivityname);

	ImGui::Text("Flags: 0x%x", seqdesc->flags);

	if (!seqdesc->AnimCount())
		return;

	ImGui::TextUnformatted("Animations:");

	for (int i = 0; i < seqdesc->AnimCount(); i++)
	{
		PreviewAnimDesc(seqdesc->anims + i, i);
	}
}