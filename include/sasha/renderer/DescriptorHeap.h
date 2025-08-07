#pragma once
#include "../utility/d3dIncludes.h"

class DescriptorHeap
{
public:
	DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptor = 1u, D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

	ID3D12DescriptorHeap* Get() const noexcept;
	UINT GetSize() const noexcept;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCPUStart(UINT index = 0u) const noexcept;
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUStart(UINT index = 0u) const noexcept;

private:
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _heap;
	UINT _descriptorSize = 0u;
};
