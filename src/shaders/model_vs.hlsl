struct VS_Input
{
    float3 position : POSITION;
    uint normal : NORMAL;
    uint4 color : COLOR;
    float2 uv : TEXCOORD;
    uint unused : UNUSED;
    int2 weights : BLENDWEIGHT;
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

StructuredBuffer<float3x4> g_boneMatrix : register(t60);
StructuredBuffer<uint> g_boneWeightsExtra : register(t1);

#define RCP_32768 (3.05175781e-05) // 1 / 32768
#define SCALEWEIGHT(w) ((w) * RCP_32768 + RCP_32768)

#define MATRIX_VEC0(m) (m._m00_m10_m20_m01)
#define MATRIX_VEC1(m) (m._m11_m21_m02_m12)
#define MATRIX_VEC2(m) (m._m22_m03_m13_m23)


VS_Output vs_main(VS_Input input)
{
    VS_Output output;


    // i am really not sure how to make the coordinate system natively behave in the same way as source
    // since source is +z up and basically every other dx or ogl app uses +y up
    // so "for now" (it will end up staying like this most likely) we have to switch the coordinates around here
    // i hate this so much
    // float3 pos = input.position.xzy;
    
    // either have to use xzy or -x y z to get accurate uvs
    float3 pos = input.position.xyz;

    // Keep track of the sum of all weights applied so far to this vertex
    float totalWeight = 0;
    
    float firstBlendWeight = SCALEWEIGHT(input.weights.x);
    totalWeight += firstBlendWeight;
    
    float4 r0 = MATRIX_VEC0(g_boneMatrix[input.blendIndices.x]) * firstBlendWeight;
    float4 r1 = MATRIX_VEC1(g_boneMatrix[input.blendIndices.x]) * firstBlendWeight;
    float4 r2 = MATRIX_VEC2(g_boneMatrix[input.blendIndices.x]) * firstBlendWeight;
    
    float3 bonePos = float3(r0.w, r1.w, r2.w);
    
    // If extra blends exist, input.weights.y forms part of the starting index for extra blends
    uint extraBlendsStartIndex = /*(input.blendIndices.z << 16) | */input.weights.y;
    for (int i = 1; i < input.blendIndices.w; ++i)
    {
        uint extraWeightPacked = g_boneWeightsExtra[extraBlendsStartIndex + (i - 1)];
        
        uint ewBoneIndex = (extraWeightPacked >> 16) & 0xffff;
        float ewWeight = SCALEWEIGHT((extraWeightPacked) & 0xffff);
        
        float3x4 ewBoneMatrix = g_boneMatrix[ewBoneIndex];
        
        r0 += MATRIX_VEC0(ewBoneMatrix) * ewWeight;
        r1 += MATRIX_VEC1(ewBoneMatrix) * ewWeight;
        r2 += MATRIX_VEC2(ewBoneMatrix) * ewWeight;
        
        bonePos += float3(r0.w, r1.w, r2.w);
        
        totalWeight += ewWeight;
    }
    
    // Final bone takes all of the rest of the weighting
    float finalBlendWeight = 1 - totalWeight;
    
    if (finalBlendWeight > 0)
    {
        float3x4 finalBoneMatrix = g_boneMatrix[input.blendIndices.y];
        
        r0 += MATRIX_VEC0(finalBoneMatrix) * finalBlendWeight;
        r1 += MATRIX_VEC1(finalBoneMatrix) * finalBlendWeight;
        r2 += MATRIX_VEC2(finalBoneMatrix) * finalBlendWeight;
    
        bonePos += float3(r0.w, r1.w, r2.w);
    }
    
    float3 worldPosition = float3(
        dot(input.position, r0.xyz) + bonePos.x,
        dot(input.position, r1.xyz) + bonePos.y,
        dot(input.position, r2.xyz) + bonePos.z);
    
    output.position = mul(float4(worldPosition, 1.f), modelMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);

    output.worldPosition = mul(float4(input.position, 1.f), modelMatrix);

    //output.color = input.color;
    
    // this is just wrong
    // output.color = float4((input.color & 0xff) / 255.f, ((input.color << 8) & 0xff) / 255.f, ((input.color << 16) & 0xff) / 255.f, ((input.color << 24) & 0xff) / 255.f);

    // todo: check if normal needs to be modified in any way like the position
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
    float3 tmp;
    
    // indices are sequential, always:
    // 0 1 2
    // 2 0 1
    // 1 2 0
    
    if (idx1 == 0)
    {
        tmp.x = vals[idx1];
        tmp.y = vals[idx2];
        tmp.z = vals[idx3];
    }
    else if (idx1 == 1)
    {
        tmp.x = vals[idx3];
        tmp.y = vals[idx1];
        tmp.z = vals[idx2];
    }
    else if (idx1 == 2)
    {
        tmp.x = vals[idx2];
        tmp.y = vals[idx3];
        tmp.z = vals[idx1];
    }
    
    output.normal = normalize(mul(tmp, modelMatrix));
    
    output.uv = input.uv;

    return output;
}
