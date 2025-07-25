#include "d3dUtil.h"

Microsoft::WRL::ComPtr<ID3D12Resource> d3dUtil::CreateBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer, const void* data, size_t byteSize)
{
	Microsoft::WRL::ComPtr<ID3D12Resource> defBuffer;
	CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC resourceDesc(CD3DX12_RESOURCE_DESC::Buffer(byteSize));
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(defBuffer.GetAddressOf())
	));

	ThrowIfFailed(device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(uploadBuffer.GetAddressOf())
	));

	D3D12_SUBRESOURCE_DATA subresourceData{};
	subresourceData.pData = data;
	subresourceData.RowPitch = byteSize;
	subresourceData.SlicePitch = byteSize;

	CD3DX12_RESOURCE_BARRIER commonToCopy = CD3DX12_RESOURCE_BARRIER
		::Transition(
			defBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_STATE_COPY_DEST
		);

	cmdList->ResourceBarrier(1, &commonToCopy);
	UpdateSubresources<1>(cmdList,
		defBuffer.Get(),
		uploadBuffer.Get(),
		0, 0, 1,
		&subresourceData
	);

	CD3DX12_RESOURCE_BARRIER copyToGeneric = CD3DX12_RESOURCE_BARRIER
		::Transition(
			defBuffer.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_GENERIC_READ	
		);
	cmdList->ResourceBarrier(1, &copyToGeneric);

	return defBuffer;
}
