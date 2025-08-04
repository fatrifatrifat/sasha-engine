#pragma once
#include "../../utility/d3dUtil.h"

struct Vertex
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT4 Color;
};

struct ConstantBuffer
{
	DirectX::XMFLOAT4X4 world = d3dUtil::Identity4x4();
};

struct PassBuffer
{
	DirectX::XMFLOAT4X4 View = d3dUtil::Identity4x4();
	DirectX::XMFLOAT4X4 InvView = d3dUtil::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = d3dUtil::Identity4x4();
	DirectX::XMFLOAT4X4 InvProj = d3dUtil::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProj = d3dUtil::Identity4x4();
	DirectX::XMFLOAT4X4 InvViewProj = d3dUtil::Identity4x4();
	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float cbPerObjectPad1 = 0.0f;
	DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;
};

struct ObjectInstance
{
	std::string meshName;         
	DirectX::XMFLOAT4X4 transform;          
};

struct SubmeshGeometry
{
	UINT _indexCount = 0;
	UINT _startIndexLocation = 0;
	INT _baseVertexLocation = 0;
};

struct MeshGeometry
{
	template <typename VertexContainer, typename IndexContainer>
	MeshGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const VertexContainer& vertices, const IndexContainer& indices)
		: _vertexStride(sizeof(Vertex))
		, _vertexByteSize(vertices.size() * _vertexStride)
		, _indexByteSize(indices.size() * sizeof(std::uint16_t))
	{
		ThrowIfFailed(D3DCreateBlob(_vertexByteSize, &_vertexCPU));
		CopyMemory(_vertexCPU->GetBufferPointer(), vertices.data(), _vertexByteSize);

		ThrowIfFailed(D3DCreateBlob(_indexByteSize, &_indexCPU));
		CopyMemory(_indexCPU->GetBufferPointer(), indices.data(), _indexByteSize);

		_vertexGPU = d3dUtil::CreateBuffer(device, cmdList, _vertexUploader, vertices.data(), _vertexByteSize);
		_indexGPU = d3dUtil::CreateBuffer(device, cmdList, _indexUploader, indices.data(), _indexByteSize);
	}

	std::string Name;

	Microsoft::WRL::ComPtr<ID3DBlob> _vertexCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> _indexCPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> _vertexGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> _indexGPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> _vertexUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> _indexUploader = nullptr;

	UINT _vertexStride = 0;
	UINT _vertexByteSize = 0;
	DXGI_FORMAT _indexFormat = DXGI_FORMAT_R16_UINT;
	UINT _indexByteSize = 0;

	std::unordered_map<std::string, SubmeshGeometry> _subGeometry;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const noexcept
	{
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = _vertexGPU->GetGPUVirtualAddress();
		vbv.StrideInBytes = _vertexStride;
		vbv.SizeInBytes = _vertexByteSize;

		return vbv;
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView() const noexcept
	{
		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = _indexGPU->GetGPUVirtualAddress();
		ibv.Format = _indexFormat;
		ibv.SizeInBytes = _indexByteSize;

		return ibv;
	}

	void DisposeUploaders() noexcept
	{
		_vertexUploader = nullptr;
		_indexUploader = nullptr;
	}
};

struct RenderItem
{
	DirectX::XMFLOAT4X4 _world = d3dUtil::Identity4x4();

	UINT _cbObjIndex = -1;
	MeshGeometry* _mesh = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY _primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT _indexCount = 0u;
	UINT _startIndex = 0u;
	UINT _baseVertex = 0u;
};

