// SRC Symbol Name: s_PreviewVertexShader

struct VS_Input
{
    float3 position : POSITION;
    uint normal : NORMAL;
    uint4 color : COLOR;
    float2 uv : TEXCOORD;
    uint weights : UNUSED;
    int2 weights_2_electric_boogaloo : BLENDWEIGHT;
    uint4 blendIndices : BLENDINDICES;
};

struct VS_Output
{
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

cbuffer VS_TransformConstants : register(b0)
{
    float4x4 modelMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
};

struct VertexWeight_t
{
    float weight;
    int bone;
};

StructuredBuffer<VertexWeight_t> g_weights : register(t61);

StructuredBuffer<float4x4> g_boneMatrix : register(t60);

VS_Output vs_main(VS_Input input)
{
    VS_Output output;

    bool sign = (input.normal >> 28) & 1;

    float val1 = sign ? -255.f : 255.f; // normal value 1
    float val2 = ((input.normal >> 19) & 0x1FF) - 256.f;
    float val3 = ((input.normal >> 10) & 0x1FF) - 256.f;

    int idx1 = (input.normal >> 29) & 3;
    int idx2 = (0x124u >> (2 * idx1 + 2)) & 3;
    int idx3 = (0x124u >> (2 * idx1 + 4)) & 3;

	// normalise the normal
    float normalised = rsqrt((255.f * 255.f) + (val2 * val2) + (val3 * val3));

    float3 vals = float3(val1 * normalised, val2 * normalised, val3 * normalised);
    float3 normal;

    // indices are sequential, always:
    // 0 1 2
    // 2 0 1
    // 1 2 0

    if (idx1 == 0)
    {
        normal.x = vals[idx1];
        normal.y = vals[idx2];
        normal.z = vals[idx3];
    }
    else if (idx1 == 1)
    {
        normal.x = vals[idx3];
        normal.y = vals[idx1];
        normal.z = vals[idx2];
    }
    else // idx1 == 2
    {
        normal.x = vals[idx2];
        normal.y = vals[idx3];
        normal.z = vals[idx1];
    }

    uint weightCount = input.weights & 0xFF;
    uint weightIndex = (input.weights >> 8) & 0xFFFFFF; // technically doesn't need the mask but might as well be sure!

    float3 weightedPos = float3(0.f, 0.f, 0.f);
    float3 weightedNml = float3(0.f, 0.f, 0.f);

    for (uint i = 0; i < weightCount; ++i)
    {
        VertexWeight_t wgt = g_weights[weightIndex + i];
        float4x4 boneMat = g_boneMatrix[wgt.bone];

        weightedPos += wgt.weight * mul(float4(input.position, 1.f), boneMat).xyz;
        weightedNml += wgt.weight * mul(normal, (float3x3)boneMat);
    }

    float4 worldPos = float4(weightedPos.xzy, 1.f);

    output.position = mul(worldPos, modelMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);

    output.worldPosition = worldPos.xyz;

    output.color = float4(input.color.r / 255.f, input.color.g / 255.f, input.color.b / 255.f, input.color.a / 255.f);

    output.normal = normalize(mul(weightedNml.xzy, (float3x3)modelMatrix));

    output.uv = input.uv;

    return output;
}
