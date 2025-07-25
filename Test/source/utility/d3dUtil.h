#pragma once
#include "d3dIncludes.h"

namespace d3dUtil
{
	inline DirectX::XMFLOAT4X4 Identity4x4()
	{
		DirectX::XMFLOAT4X4 result;
		DirectX::XMStoreFloat4x4(&result, DirectX::XMMatrixIdentity());
		return result;
	}

	inline UINT CalcConstantBufferSize(UINT size)
	{
		return (size + 255) & ~255;
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBuffer(ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer,
		const void* data,
		UINT64 byteSize);

	template<typename T>
	class UploadBuffer
	{
	public:
		UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer = false)
			: _isConstantBuffer(isConstantBuffer)
		{
			_byteSize = sizeof(T);

			if (_isConstantBuffer)
				_byteSize = CalcConstantBufferSize(_byteSize);

			CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
			CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(_byteSize * elementCount);
			ThrowIfFailed(device->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&_uploadBuffer)
			));

			ThrowIfFailed(_uploadBuffer->Map(
				0,
				nullptr,
				reinterpret_cast<void**>(&_mappedData)
			));
		}

		UploadBuffer(const UploadBuffer& rhs) = delete;
		UploadBuffer& operator=(const UploadBuffer& rhs) = delete;

		~UploadBuffer()
		{
			if (_uploadBuffer)
				_uploadBuffer->Unmap(0, nullptr);

			_mappedData = nullptr;
		}

		ID3D12Resource* GetResource() const
		{
			return _uploadBuffer.Get();
		}

		void CopyData(UINT elementCount, const T& data)
		{
			memcpy(&_mappedData, &data, elementCount * sizeof(T));
		}

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> _uploadBuffer;
		BYTE* _mappedData = nullptr;
		UINT _byteSize = 0u;
		bool _isConstantBuffer = false;
	};

	Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
		const std::wstring& filename,
		const D3D_SHADER_MACRO* defines,
		const std::string& entrypoint,
		const std::string& target);
}
