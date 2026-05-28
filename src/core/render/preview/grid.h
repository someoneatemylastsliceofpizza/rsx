#pragma once

#include <core/render/dxshader.h>
#include <core/math/vector.h>

struct GridVertex_t
{
	float x, y, z;
	uint32_t color;

	constexpr GridVertex_t() {};
	constexpr GridVertex_t(float x, float y, float z, uint32_t color) : x(x), y(y), z(z), color(color) {};
};
class CShader;

template<int N, int SZ=1>
struct PreviewGrid_t
{
	constexpr PreviewGrid_t() : numVertices(4*(N+1))
	{
		constexpr float minCoord = (-(N / 2)) * SZ;
		constexpr float maxCoord = (N / 2) * SZ;
		constexpr uint16_t horizontalVertsStart = (2 * (N + 1));

		uint16_t numVertsWritten = 0;
		for (float i = minCoord; i <= maxCoord; i += SZ)
		{
			constexpr uint32_t lineColour = 0x919191FF;
			vertices[numVertsWritten] = { i, 0, maxCoord, lineColour };
			vertices[numVertsWritten+1] = { i, 0, minCoord, lineColour };

			vertices[numVertsWritten + horizontalVertsStart] = { minCoord, 0, i, lineColour };
			vertices[numVertsWritten + horizontalVertsStart + 1] = { maxCoord, 0, i, lineColour };

			numVertsWritten += 2;
		}
	};

	PreviewGrid_t(const Vector& origin, const QAngle& angle) : numVertices(4 * (N + 1))
	{
		constexpr float    minCoord = (-(N / 2)) * SZ;
		constexpr float    maxCoord = ((N / 2)) * SZ;
		constexpr uint16_t horizontalVertsStart = 2 * (N + 1);

		const float cp = cosf(angle.x * s_DEG2RAD_CONST), sp = sinf(angle.x * s_DEG2RAD_CONST);
		const float cy = cosf(angle.y * s_DEG2RAD_CONST), sy = sinf(angle.y * s_DEG2RAD_CONST);
		const float cr = cosf(angle.z * s_DEG2RAD_CONST), sr = sinf(angle.z * s_DEG2RAD_CONST);

		auto MakeVertex = [&](float vx, float vy, float vz) -> GridVertex_t
			{
				constexpr uint32_t lineColour = 0x919191FF;
				const float rx = cp * cy * vx + (sr * sp * cy - cr * sy) * vy + (cr * sp * cy + sr * sy) * vz;
				const float ry = cp * sy * vx + (sr * sp * sy + cr * cy) * vy + (cr * sp * sy - sr * cy) * vz;
				const float rz = -sp * vx + sr * cp * vy + cr * cp * vz;
				return { origin.x + rx, origin.y + ry, origin.z + rz, lineColour };
			};

		uint16_t numVertsWritten = 0;
		for (float i = minCoord; i <= maxCoord; i += SZ)
		{
			vertices[numVertsWritten] = MakeVertex(i, 0.0f, maxCoord);
			vertices[numVertsWritten + 1] = MakeVertex(i, 0.0f, minCoord);

			vertices[numVertsWritten + horizontalVertsStart] = MakeVertex(minCoord, 0.0f, i);
			vertices[numVertsWritten + horizontalVertsStart + 1] = MakeVertex(maxCoord, 0.0f, i);

			numVertsWritten += 2;
		}
	}

	GridVertex_t vertices[4*(N+1)];

	UINT numVertices;
	UINT vertexStride;

	ID3D11Buffer* vertexBuffer;

	CShader* vertexShader;
	CShader* pixelShader;

	void CreateBuffers(ID3D11Device* device)
	{
		if (!vertexBuffer)
		{
			constexpr UINT vertStride = sizeof(GridVertex_t);

			D3D11_BUFFER_DESC desc = {};

			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.ByteWidth = static_cast<UINT>(vertStride * numVertices);
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			desc.MiscFlags = 0;

			D3D11_SUBRESOURCE_DATA srd{ vertices };

			if (FAILED(device->CreateBuffer(&desc, &srd, &vertexBuffer)))
				return;

			vertexStride = vertStride;
		}
	}

	void Draw(ID3D11DeviceContext* ctx)
	{
		UINT offset = 0u;

		ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		ctx->IASetVertexBuffers(0u, 1u, &vertexBuffer, &vertexStride, &offset);

		ctx->IASetInputLayout(vertexShader->GetInputLayout());
		ctx->VSSetShader(vertexShader->Get<ID3D11VertexShader>(), nullptr, 0u);
		ctx->PSSetShader(pixelShader->Get<ID3D11PixelShader>(), nullptr, 0u);

		ctx->Draw(numVertices, 0);
	}
};