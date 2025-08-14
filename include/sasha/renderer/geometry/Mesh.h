#pragma once
#include "../../utility/d3dUtil.h"
#include "Material.h"

struct Vertex
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
};

struct ConstantBuffer
{
	DirectX::XMFLOAT4X4 world = d3dUtil::Identity4x4();
};

struct Light
{
	DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
	float FalloffStart = 1.0f;                          // point/spot light only
	DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };// directional/spot light only
	float FalloffEnd = 10.0f;                           // point/spot light only
	DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };  // point/spot light only
	float SpotPower = 64.0f;                            // spot light only
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

	DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
	Light Lights[16];
};

struct ObjectInstance
{
	std::string meshName;         
	DirectX::XMFLOAT4X4 transform;
	std::string matName;
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
		, _vertexByteSize(static_cast<UINT>(vertices.size() * _vertexStride))
		, _indexByteSize(static_cast<UINT>(indices.size() * sizeof(std::uint16_t)))
	{
		ThrowIfFailed(D3DCreateBlob(_vertexByteSize, &_vertexCPU));
		CopyMemory(_vertexCPU->GetBufferPointer(), vertices.data(), _vertexByteSize);

		ThrowIfFailed(D3DCreateBlob(_indexByteSize, &_indexCPU));
		CopyMemory(_indexCPU->GetBufferPointer(), indices.data(), _indexByteSize);

		//_vertexGPU = d3dUtil::CreateBuffer(device, cmdList, _vertexUploader, vertices.data(), _vertexByteSize);
		_vertexGPU = std::make_unique<d3dUtil::UploadBuffer<Vertex>>(device, static_cast<UINT>(vertices.size()), false);
		for (size_t i = 0; i < vertices.size(); i++)
			_vertexGPU->CopyData(static_cast<UINT>(i), vertices[i]);
		_indexGPU = d3dUtil::CreateBuffer(device, cmdList, _indexUploader, indices.data(), _indexByteSize);
	}

	std::string Name;

	Microsoft::WRL::ComPtr<ID3DBlob> _vertexCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> _indexCPU = nullptr;

	//Microsoft::WRL::ComPtr<ID3D12Resource> _vertexGPU = nullptr;
	std::unique_ptr<d3dUtil::UploadBuffer<Vertex>> _vertexGPU;
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
		vbv.BufferLocation = _vertexGPU->GetResource()->GetGPUVirtualAddress();
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
	Material* _material = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY _primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT _indexCount = 0u;
	UINT _startIndex = 0u;
	UINT _baseVertex = 0u;

	UINT _hasMoved = true;
};