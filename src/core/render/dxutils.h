#pragma once

#include <core/render/dx.h>

bool CreateD3DBuffer(
    ID3D11Device* device, ID3D11Buffer** pBuffer,
    UINT byteWidth, D3D11_USAGE usage,
    D3D11_BIND_FLAG bindFlags, int cpuAccessFlags,
    UINT miscFlags, UINT structureByteStride = 0, void* initialData = nullptr);

static const char* D3D11_BLEND_NAMES[] = {
    "D3D11_BLEND_INVALID",
    "D3D11_BLEND_ZERO",
    "D3D11_BLEND_ONE",
    "D3D11_BLEND_SRC_COLOR",
    "D3D11_BLEND_INV_SRC_COLOR",
    "D3D11_BLEND_SRC_ALPHA",
    "D3D11_BLEND_INV_SRC_ALPHA",
    "D3D11_BLEND_DEST_ALPHA",
    "D3D11_BLEND_INV_DEST_ALPHA",
    "D3D11_BLEND_DEST_COLOR",
    "D3D11_BLEND_INV_DEST_COLOR",
    "D3D11_BLEND_SRC_ALPHA_SAT",
    "D3D11_BLEND_BLEND_FACTOR",
    "D3D11_BLEND_INV_BLEND_FACTOR",
    "D3D11_BLEND_SRC1_COLOR",
    "D3D11_BLEND_INV_SRC1_COLOR",
    "D3D11_BLEND_SRC1_ALPHA",
    "D3D11_BLEND_INV_SRC1_ALPHA",
};

static const char* D3D11_BLEND_OP_NAMES[] = {
    "D3D11_BLEND_OP_INVALID",
    "D3D11_BLEND_OP_ADD",
    "D3D11_BLEND_OP_SUBTRACT",
    "D3D11_BLEND_OP_REV_SUBTRACT",
    "D3D11_BLEND_OP_MIN",
    "D3D11_BLEND_OP_MAX",
};

bool GenerateTexture2D(const uint32_t pixel, const uint8_t pixelSize, DXGI_FORMAT format, UINT width, UINT height, UINT arraySize, UINT bindFlags, bool isCube, ID3D11Texture2D** o_texture2d, ID3D11ShaderResourceView** o_srv);

class CDXDrawData;
class CPreviewDrawData
{
public:
    CPreviewDrawData() : drawData(nullptr), guid(0ull), activeMonitor(0u), activeLODLevel(0u) {}
    ~CPreviewDrawData();

    CDXDrawData* drawData;
    uint64_t guid;
    uint32_t activeMonitor;
    uint8_t activeLODLevel;

    inline void FreeDrawData();
    bool CheckForMonitorChange();

    inline void UpdateAssetInfo(CDXDrawData* const assetDrawData, const uint64_t assetGUID, const uint8_t assetLOD)
    {
        drawData = assetDrawData;
        guid = assetGUID;
        activeLODLevel = assetLOD;
    }

    CDXDrawData* const GetDrawData() const { return drawData; }
};