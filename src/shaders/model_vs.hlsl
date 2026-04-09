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
    //float3 pos = input.position.xyz;
   
    uint weightCount = input.weights & 0xFF;
    uint weightIndex = (input.weights >> 8) & 0xFFFFFF; // technically doesn't need the mask but might as well be sure!
    
    float4 pos = float4(0, 0, 0, 0);
    
    
    int i;
    for (i = 0; i < weightCount; ++i)
    {
        VertexWeight_t wgt = g_weights[weightIndex + i];
        
        float4 newPosDelta = wgt.weight * mul(float4(input.position.xzy, 1.f), g_boneMatrix[wgt.bone]);
        
        pos += newPosDelta;
    }
    
    output.position = mul(pos, modelMatrix);
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);

    output.worldPosition = pos.xyz;//mul(float4(pos, 1.f), modelMatrix);

    output.color = float4(input.color.r / 255.f, input.color.g / 255.f, input.color.b / 255.f, input.color.a / 255.f);
    
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
