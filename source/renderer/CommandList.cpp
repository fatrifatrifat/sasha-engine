#include "../../include/sasha/renderer/CommandList.h"

CommandList::CommandList(ID3D12Device* device, ID3D12PipelineState* pso, D3D12_COMMAND_LIST_TYPE type)
{
	ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&_cmdAlloc)));
	ThrowIfFailed(device->CreateCommandList(0u, type, _cmdAlloc.Get(), pso, IID_PPV_ARGS(&_cmdList)));
}

void CommandList::Reset(ID3D12PipelineState* pso)
{
	ThrowIfFailed(_cmdList->Reset(_cmdAlloc.Get(), pso));
}

void CommandList::Reset(ID3D12CommandAllocator* cmdAlloc, ID3D12PipelineState* pso)
{
	ThrowIfFailed(_cmdList->Reset(cmdAlloc, pso));
}

void CommandList::ChangeResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES base, D3D12_RESOURCE_STATES change, UINT numBarrier)
{
	CD3DX12_RESOURCE_BARRIER trans = CD3DX12_RESOURCE_BARRIER::Transition(resource, base, change);
	_cmdList->ResourceBarrier(numBarrier, &trans);
}

ID3D12GraphicsCommandList* CommandList::Get() const
{
	return _cmdList.Get();
}

ID3D12CommandAllocator* CommandList::GetCmdAlloc() const
{
	return _cmdAlloc.Get();
}
