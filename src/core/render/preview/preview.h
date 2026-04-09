#pragma once

// Vertex Shader: Shader Resources
constexpr int VSRSRC_BONE_MATRIX            = 60u; // g_boneMatrix
constexpr int VSRSRC_BONE_MATRIX_PREV_FRAME = 62u; // g_boneMatrixPrevFrame

// Pixel Shader: Shader Resources
constexpr int PSRSRC_CUBEMAP              = 40u; // IndirectSpecularTextureCubeArray
constexpr int PSRSRC_CSMDEPTHATLASSAMPLER = 43u; // CSMAtlasDepthSampler
constexpr int PSRSRC_SHADOWMAP            = 44u; // t_shadowMaps
constexpr int PSRSRC_CLOUDMASK            = 45u; // cloudMaskTexture
constexpr int PSRSRC_STATICSHADOWTEXTURE  = 46u; // staticShadowTexture
constexpr int PSRSRC_GLOBAL_LIGHTS        = 62u; // s_globalLights
constexpr int PSRSRC_CUBEMAP_SAMPLES      = 72u; // g_cubemapSamples

// Shaders for debug primitives (lines etc.)
constexpr static const char s_PrimitivePixelShader[]{
	"struct VS_Output\n"
	"{\n"
	"    float4 position : SV_POSITION;\n"
	"    float4 color : COLOR;\n"
	"};\n"
	"float4 ps_main(VS_Output input) : SV_Target\n"
	"{\n"
	"    return input.color;\n"
	"}"
};

constexpr static const char s_PrimitiveVertexShader[]{
	"struct VS_Input\n"
	"{\n"
	"    float3 position : POSITION;\n"
	"    uint4 color : COLOR;\n"
	"};\n"
	"struct VS_Output\n"
	"{\n"
	"    float4 position : SV_POSITION;\n"
	"    float4 color : COLOR;\n"
	"};\n"
	"cbuffer VS_TransformConstants : register(b0)\n"
	"{\n"
	"    float4x4 modelMatrix;\n"
	"    float4x4 viewMatrix;\n"
	"    float4x4 projectionMatrix;\n"
	"};\n"
	"VS_Output vs_main(VS_Input input)\n"
	"{\n"
	"    VS_Output output;\n"
	"    float3 pos = float3(input.position.x, input.position.y, input.position.z);\n"
	"    output.position = mul(float4(pos, 1.f), modelMatrix);\n"
	"    output.position = mul(output.position, viewMatrix);\n"
	"    output.position = mul(output.position, projectionMatrix);\n"
	"    output.color = float4(input.color.r / 255.f, input.color.g / 255.f, input.color.b / 255.f, input.color.a / 255.f);\n"
	"    return output;\n"
	"}"
};

static D3D11_INPUT_ELEMENT_DESC s_PrimitiveInputLayout[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

struct PrimitiveVertex_t
{
	Vector position;
	uint32_t color;
};

void Preview_MapTransformsBuffer(CDXDrawData* drawData);
void Preview_MapModelInstanceBuffer(CDXDrawData* drawData);

// Preview types
void Preview_Model(CDXDrawData* drawData, float dt);
void Preview_Texture(CDXDrawData* drawData, float dt);
