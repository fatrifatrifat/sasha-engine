#pragma once
#include "../utility/d3dIncludes.h"

class CommandList
{
public:
	CommandList(ID3D12Device* device, ID3D12PipelineState* pso = nullptr, D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);

	void Reset(ID3D12PipelineState* pso = nullptr) const;
	void Reset(ID3D12CommandAllocator* cmdAlloc, ID3D12PipelineState* pso = nullptr) const;
	void ChangeResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES base, D3D12_RESOURCE_STATES change, UINT numBarrier = 1u) const noexcept;

	ID3D12GraphicsCommandList* Get() const noexcept;
	ID3D12CommandAllocator* GetCmdAlloc() const noexcept;

private:
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _cmdList;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _cmdAlloc;
};
