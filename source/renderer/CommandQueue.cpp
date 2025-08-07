#include "../../include/sasha/renderer/CommandQueue.h"
#include <stdlib.h>

CommandQueue::CommandQueue(ID3D12Device* device,
	D3D12_COMMAND_QUEUE_FLAGS queueFlags, 
	D3D12_COMMAND_LIST_TYPE type,
	D3D12_FENCE_FLAGS fenceFlags)
{
	D3D12_COMMAND_QUEUE_DESC cqDesc{};
	cqDesc.Flags = queueFlags;
	cqDesc.Type = type;

	ThrowIfFailed(device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&_cmdQueue)));

	ThrowIfFailed(device->CreateFence(_currFence, fenceFlags, IID_PPV_ARGS(&_fence)));
}

ID3D12CommandQueue* CommandQueue::Get() const noexcept
{
	return _cmdQueue.Get();
}

ID3D12Fence* CommandQueue::GetFence() const noexcept
{
	return _fence.Get();
}

UINT64& CommandQueue::GetCurrFence() noexcept
{
	return _currFence;
}

void CommandQueue::ExecuteCmdList(ID3D12GraphicsCommandList* cmdList) const
{
	ThrowIfFailed(cmdList->Close());
	ID3D12CommandList* cmdLists[] = { cmdList };
	_cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
}

void CommandQueue::Signal() const
{
	ThrowIfFailed(_cmdQueue->Signal(_fence.Get(), _currFence));
}

void CommandQueue::Flush() const
{
	// Synchronizes the CPU and the GPU to a certain command list
	_currFence++;
	_cmdQueue->Signal(_fence.Get(), _currFence);

	if (_fence->GetCompletedValue() < _currFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		assert(eventHandle != 0);
		_fence->SetEventOnCompletion(_currFence, eventHandle);
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}
