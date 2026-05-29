#pragma once

struct UIAssetArgCluster_t
{
    uint16_t argIndex;
    uint16_t argCount;

    uint8_t hashMul;
    uint8_t hashAdd;

    uint16_t unk_6;
    uint16_t defaultValuesSize; // equal to defaultValuesSize in the asset header

    uint16_t ruiDataStructSize; // equal to ruiDataStructSize in the asset header

    char gap_C[6];
};

enum UIAssetArgType_t : uint8_t
{
    UI_ARG_TYPE_NONE = 0,
    UI_ARG_TYPE_STRING = 0x1,
    UI_ARG_TYPE_ASSET = 0x2,
    UI_ARG_TYPE_BOOL = 0x3,
    UI_ARG_TYPE_INT = 0x4,
    UI_ARG_TYPE_FLOAT = 0x5,
    UI_ARG_TYPE_FLOAT2 = 0x6,
    UI_ARG_TYPE_FLOAT3 = 0x7,
    UI_ARG_TYPE_COLOR_ALPHA = 0x8,
    UI_ARG_TYPE_GAMETIME = 0x9,
    UI_ARG_TYPE_WALLTIME = 0xA,
    UI_ARG_TYPE_UIHANDLE = 0xB,
    UI_ARG_TYPE_IMAGE = 0xC,
    UI_ARG_TYPE_FONT_FACE = 0xD,
    UI_ARG_TYPE_FONT_HASH = 0xE,
    UI_ARG_TYPE_ARRAY = 0xF,
};

// type names for preview
static const char* const s_UIArgTypeNames[] = {
    "NONE",
    "STRING",
    "ASSET",
    "BOOL",
    "INT",
    "FLOAT",
    "FLOAT2",
    "FLOAT3",
    "COLOR ALPHA",
    "GAMETIME",
    "WALLTIME",
    "UIHANDLE",
    "IMAGE",
    "FONT FACE",
    "FONT HASH",
    "ARRAY",
};

// type names for export
static const char* const s_typeNamesRUI[] = {
    "",
    "string",
    "asset",
    "bool",
    "int",
    "float",
    "float2",
    "float3",
    "Color", // float4
    "GameTime",
    "WallTime",
    "UIHandle",
    "Image",
    "FontFace",
    "FontHash",
    "array"
};

// terrifying pred datamap flashbacks
static uint8_t s_typeSizes[] = {
    0,
    sizeof(char*),   // string
    sizeof(char*),   // asset
    sizeof(int8_t),  // boolean
    sizeof(int32_t), // int
    sizeof(float),   // float
    2 * sizeof(float), // float2
    3 * sizeof(float), // float3
    4 * sizeof(float), // color (rgba)
    sizeof(float),   // gametime
    sizeof(float),   // walltime
    sizeof(int8_t),  // uihandle - this type has a different size on r2/r5! yippee!
    sizeof(char*),   // image
    0,               // fontface (idk)
    sizeof(int32_t), // fonthash
    0,               // array (idk)
};

static uint32_t s_typeColours[] = {
    0,
    0x3000BFFF, // #FFBF0030 - string
    0x3000BFFF, // #FFBF0030 - asset (string type)
    0x3000FF00, // #00FF0030 - boolean
    0x30FF0000, // #0000FF30 - int
    0x30D4E6B2, // #B2E6D430 - float
    0x30D4E6B2, // #B2E6D430 - float2
    0x30D4E6B2, // #B2E6D430 - float3
    0x30D4E6B2, // #B2E6D430 - color (rgba)
    0x30BAE883, // #83E8BA30 - gametime
    0x30BAE883, // #83E8BA30 - walltime (time type)
    0x300000FF, // #FF000030 - ui handle (red bc variable size based on game, i could determine it by asset ver but whatever)
    0x3000BFFF, // #FFBF0030 - image (string type)
    0, // fontface (idk)
    0x3000FFFF, // #FFFF0030 - fonthash
    0, // array
};

union UIAssetArgValue_t
{
    char* rawptr;
    char** string;
    bool* boolean;
    int* integer;
    float* fpn;
    int64_t* integer64;
};

struct UIAssetArg_t
{
    UIAssetArgType_t type;

    uint8_t unk_1;

    uint16_t dataOffset;
    uint16_t nameOffset;

    uint16_t shortHash;
};

struct UIColor_t
{
    uint16_t r;
    uint16_t g;
    uint16_t b;
    uint16_t a;
};

enum UIAssetStyleTypes_e
{
    TYPE_TEXT = 0,
    TYPE_UNK1 = 1,
    TYPE_ELLIPSE = 2,

};

struct UIAssetStyleDescriptor_t
{
    uint16_t type;
    UIColor_t colors[3];

    char gap_1A[4];

    uint16_t fontIndex_1E;

    uint16_t unk_20[4];

    uint16_t textSize; // word_28
    uint16_t textStretch; //  word_2A

    uint16_t unk_2C[4];

    char gap_end[16];
};

static_assert(sizeof(UIAssetStyleDescriptor_t) == 0x44); // observed size from s10 apex
static_assert(offsetof(UIAssetStyleDescriptor_t, fontIndex_1E) == 30);

struct UIAssetMapping_t
{
    uint32_t dataCount;

    uint16_t unk_4;
    uint16_t unk_6;

    float* data;
};

struct UIAssetHeader_t
{
    const char* name;

    void* defaultValues;
    uint8_t* transformData;

    float elementWidth;
    float elementHeight;
    float elementWidthRatio;
    float elementHeightRatio;

    const char* argNames;
    UIAssetArgCluster_t* argClusters;
    UIAssetArg_t* args;
    int16_t argCount;

    int16_t unk_10; // count

    uint16_t ruiDataStructSize;
    uint16_t defaultValuesSize;
    uint16_t styleDescriptorCount;

    uint16_t unk_A4;

    uint16_t renderJobCount;
    uint16_t argClusterCount;

    UIAssetStyleDescriptor_t* styleDescriptors;
    uint8_t* renderJobs;
    UIAssetMapping_t* mappingData;
};

class UIAsset
{
public:
    UIAsset(UIAssetHeader_t* hdr) : name(hdr->name), elementWidth(hdr->elementWidth), elementHeight(hdr->elementHeight), elementWidthRatio(hdr->elementWidthRatio), elementHeightRatio(hdr->elementHeightRatio),
        argNames(hdr->argNames),  args(hdr->args), argClusters(hdr->argClusters), argDefaultValues(hdr->defaultValues), styleDescriptors(hdr->styleDescriptors), argCount(hdr->argCount), argClusterCount(hdr->argClusterCount),
        argDefaultValueSize(hdr->defaultValuesSize), styleDescCount(hdr->styleDescriptorCount),
        ruiDataStructSize(hdr->ruiDataStructSize), minArgDataOffset(0)
    {};

    const char* name;

    float elementWidth;
    float elementHeight;
    float elementWidthRatio;
    float elementHeightRatio;

    const char* argNames;
    UIAssetArg_t* args;
    UIAssetArgCluster_t* argClusters;
    void* argDefaultValues;
    UIAssetStyleDescriptor_t* styleDescriptors;

    int16_t argCount;
    uint16_t argClusterCount;
    uint16_t argDefaultValueSize;

    uint16_t ruiDataStructSize;
    uint16_t styleDescCount;

    uint16_t minArgDataOffset;
};