#include "../../include/sasha/renderer/DescriptorHeap.h"

DescriptorHeap::DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptor, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
	_descriptorSize = device->GetDescriptorHandleIncrementSize(type);

	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.Flags = flags;
	desc.NodeMask = 0u;
	desc.NumDescriptors = numDescriptor;
	desc.Type = type;
	ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_heap)));
}

ID3D12DescriptorHeap* DescriptorHeap::Get() const noexcept
{
	return _heap.Get();
}

UINT DescriptorHeap::GetSize() const noexcept
{
	return _descriptorSize;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCPUStart(UINT index) const noexcept
{
	auto address = CD3DX12_CPU_DESCRIPTOR_HANDLE(_heap->GetCPUDescriptorHandleForHeapStart());
	address.Offset(index * _descriptorSize);
	return address;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGPUStart(UINT index) const noexcept
{
	auto address = CD3DX12_GPU_DESCRIPTOR_HANDLE(_heap->GetGPUDescriptorHandleForHeapStart());
	address.Offset(index * _descriptorSize);
	return address;
}
