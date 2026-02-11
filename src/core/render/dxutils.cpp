#include <pch.h>

#include <d3d11.h>
#include <game/rtech/utils/utils.h>

#include <core/render/dx.h>
#include <core/render/dxutils.h>

extern CDXParentHandler* g_dxHandler;

bool CreateD3DBuffer(
    ID3D11Device* device, ID3D11Buffer** pBuffer,
    UINT byteWidth, D3D11_USAGE usage,
    D3D11_BIND_FLAG bindFlags, int cpuAccessFlags,
    UINT miscFlags, UINT structureByteStride, void* initialData)
{
    assert(pBuffer);

    if (!device)
        return false;

    D3D11_BUFFER_DESC desc = {};

    desc.Usage = usage;
    desc.ByteWidth = bindFlags & D3D11_BIND_CONSTANT_BUFFER ? IALIGN(byteWidth, 16) : byteWidth;
    desc.BindFlags = bindFlags;
    desc.CPUAccessFlags = cpuAccessFlags;
    desc.MiscFlags = miscFlags;
    desc.StructureByteStride = structureByteStride;

    HRESULT hr;
    if (initialData)
    {
        D3D11_SUBRESOURCE_DATA resource = { initialData };
        hr = device->CreateBuffer(&desc, &resource, pBuffer);
    }
    else
    {
        hr = device->CreateBuffer(&desc, NULL, pBuffer);
    }

    assert(SUCCEEDED(hr));

    return SUCCEEDED(hr);
}

bool GenerateTexture2D(uint32_t pixel, const uint8_t pixelSize, DXGI_FORMAT format, UINT width, UINT height, UINT arraySize, UINT bindFlags, bool isCube, ID3D11Texture2D** o_texture2d, ID3D11ShaderResourceView** o_srv)
{
    char* pixelDataPerImage = new char[width * height * pixelSize];

    // If the pixel data is zero, just use memset
    if (pixel == 0)
        memset(pixelDataPerImage, 0, width * height * pixelSize);
    else
    {
        // Loop over the whole pixel buffer and copy our value into it
        for (size_t i = 0; i < width * height; ++i)
        {
            memcpy(&pixelDataPerImage[i * pixelSize], &pixel, pixelSize);
        }
    }


    D3D11_TEXTURE2D_DESC textureDesc;
    textureDesc.Height = height;
    textureDesc.Width = width;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = isCube ? 6 * arraySize : arraySize;
    textureDesc.Format = format;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | bindFlags;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = isCube ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;

    D3D11_SUBRESOURCE_DATA* subData = new D3D11_SUBRESOURCE_DATA[textureDesc.ArraySize];

    for (UINT i = 0; i < textureDesc.ArraySize; i++) {

        subData[i].pSysMem = pixelDataPerImage;
        subData[i].SysMemPitch = (UINT)(width * pixelSize);
        subData[i].SysMemSlicePitch = 0;
    }

    // Create the empty texture.
    HRESULT hr = g_dxHandler->GetDevice()->CreateTexture2D(&textureDesc, (D3D11_SUBRESOURCE_DATA*)subData, o_texture2d);
    if (FAILED(hr))
        return false;

    delete[] pixelDataPerImage;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};

    // this is bad!
    if (format == DXGI_FORMAT_R24G8_TYPELESS) format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    if (format == DXGI_FORMAT_R16_TYPELESS) format = DXGI_FORMAT_R16_UNORM;

    srvDesc.Format = format;
    if (!isCube)
    {
        srvDesc.ViewDimension = arraySize == 1 ? D3D11_SRV_DIMENSION_TEXTURE2D : D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        
        if (arraySize == 1)
        {
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = 1;
        }
        else
        {
            srvDesc.Texture2DArray.ArraySize = arraySize;
            srvDesc.Texture2DArray.FirstArraySlice = 0;
            srvDesc.Texture2DArray.MostDetailedMip = 0;
            srvDesc.Texture2DArray.MipLevels = 1;
        }
    }
    else
    {
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;

        srvDesc.TextureCubeArray.First2DArrayFace = 0;
        srvDesc.TextureCubeArray.MipLevels = 1;
        srvDesc.TextureCubeArray.MostDetailedMip = 0;
        srvDesc.TextureCubeArray.NumCubes = arraySize;
        
    }

    hr = g_dxHandler->GetDevice()->CreateShaderResourceView(*o_texture2d, &srvDesc, o_srv);
    if (FAILED(hr))
        return false;

    return true;
}

CPreviewDrawData::~CPreviewDrawData()
{
    FreeDrawData();
}

inline void CPreviewDrawData::FreeDrawData()
{
    FreeAllocVar(drawData);
    drawData = nullptr;
}

// check if the monitor changed, and update accordingly
bool CPreviewDrawData::CheckForMonitorChange()
{
    const uint32_t dxHandlerActiveMonitor = g_dxHandler->GetActiveMonitor();
    if (activeMonitor == dxHandlerActiveMonitor)
        return false;

    if (g_dxHandler->MonitorHasSameAdapter(dxHandlerActiveMonitor, activeMonitor))
    {
        activeMonitor = dxHandlerActiveMonitor;
        return false;
    }

    activeMonitor = dxHandlerActiveMonitor;
    FreeDrawData();

    return true;
}