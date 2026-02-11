#include <pch.h>
#include <core/render/dx.h>
#include <core/render/preview/preview.h>
#include <core/input/input.h>

extern CDXParentHandler* g_dxHandler;

void Preview_MapTransformsBuffer(CDXDrawData* drawData)
{
	if (!drawData->transformsBuffer)
	{
		D3D11_BUFFER_DESC desc{};

		constexpr UINT transformsBufferSizeAligned = IALIGN(sizeof(VS_TransformConstants), 16);

		desc.ByteWidth = transformsBufferSizeAligned;

		// make sure this buffer can be updated every frame
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		// const buffer
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

		g_dxHandler->GetDevice()->CreateBuffer(&desc, NULL, &drawData->transformsBuffer);
	}

	D3D11_MAPPED_SUBRESOURCE resource;
	g_dxHandler->GetDeviceContext()->Map(
		drawData->transformsBuffer, 0,
		D3D11_MAP_WRITE_DISCARD, 0,
		&resource
	);

	CDXCamera* const camera = g_dxHandler->GetCamera();
	if (g_pInput->GetKeyState(CInput::KeyCode_t::KEY_HOME))
		drawData->position = camera->position;

	const XMMATRIX view = camera->GetViewMatrix();
	const XMMATRIX model = XMMatrixTranslationFromVector(drawData->position.AsXMVector());
	const XMMATRIX projection = g_dxHandler->GetProjMatrix();

	VS_TransformConstants* const transforms = reinterpret_cast<VS_TransformConstants*>(resource.pData);
	transforms->modelMatrix = XMMatrixTranspose(model);
	transforms->viewMatrix = XMMatrixTranspose(view);
	transforms->projectionMatrix = XMMatrixTranspose(projection);

	g_dxHandler->GetDeviceContext()->Unmap(drawData->transformsBuffer, 0);
}

void Preview_MapModelInstanceBuffer(CDXDrawData* drawData)
{

	// Map model-specific transforms data (CBufModelInstance/VS_TransformConstants)
	if (!drawData->modelInstanceBuffer)
	{
		D3D11_BUFFER_DESC desc{};

		constexpr UINT miBufSizeAligned = IALIGN(sizeof(CBufModelInstance), 16);

		desc.ByteWidth = miBufSizeAligned;

		// make sure this buffer can be updated every frame
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		// const buffer
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

		g_dxHandler->GetDevice()->CreateBuffer(&desc, NULL, &drawData->modelInstanceBuffer);
	}

	D3D11_MAPPED_SUBRESOURCE resource;
	g_dxHandler->GetDeviceContext()->Map(
		drawData->modelInstanceBuffer, 0,
		D3D11_MAP_WRITE_DISCARD, 0,
		&resource
	);

	CDXCamera* const camera = g_dxHandler->GetCamera();
	const XMMATRIX view = camera->GetViewMatrix();
	const XMMATRIX model = XMMatrixTranslationFromVector(drawData->position.AsXMVector());
	const XMMATRIX projection = g_dxHandler->GetProjMatrix();

	CBufModelInstance* const modelInstance = reinterpret_cast<CBufModelInstance*>(resource.pData);
	CBufModelInstance::ModelInstance& mi = modelInstance->c_modelInst;

	mi.objectToCameraRelativePrevFrame = mi.objectToCameraRelative;

	// sub_18001FB40 in r2
	/*XMMATRIX modelMatrix = {};
	memcpy(&modelMatrix, &parsedData->bones[0].poseToBone, sizeof(ModelBone_t::poseToBone));

	modelMatrix.r[0].m128_f32[3] += camera->position.x;
	modelMatrix.r[1].m128_f32[3] += camera->position.y;
	modelMatrix.r[2].m128_f32[3] += camera->position.z;
	modelMatrix.r[3].m128_f32[3] = 1.f;

	modelMatrix -= XMMatrixRotationRollPitchYaw(camera->rotation.x, camera->rotation.y, camera->rotation.z);

	modelMatrix = XMMatrixTranspose(modelMatrix);*/

	mi.diffuseModulation = { 1.f, 1.f, 1.f, 1.f };
	mi.cubemapID = 1;
	mi.lighting.packedLightData = { 16777215, 4718655, 4294901845u, 0 };

	mi.lighting.ambientSH[0] = XMFLOAT4(0.10217f, -0.39209f, -0.04834f, 0.29187f);
	mi.lighting.ambientSH[1] = XMFLOAT4(0.05273f, -0.35046f, 0.04895f, 0.26135f);
	mi.lighting.ambientSH[2] = XMFLOAT4(0.04077f, -0.36914f, 0.09009f, 0.26636f);

	XMStoreFloat3x4(&mi.objectToCameraRelative, model);

	g_dxHandler->GetDeviceContext()->Unmap(drawData->modelInstanceBuffer, 0);
}