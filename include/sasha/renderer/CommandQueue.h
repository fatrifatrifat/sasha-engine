#pragma once
#include "../utility/d3dIncludes.h"

class CommandQueue
{
public:
	CommandQueue(ID3D12Device* device,
		D3D12_COMMAND_QUEUE_FLAGS queueFlags = D3D12_COMMAND_QUEUE_FLAG_NONE,
		D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		D3D12_FENCE_FLAGS fenceFlags = D3D12_FENCE_FLAG_NONE);

	void ExecuteCmdList(ID3D12GraphicsCommandList* cmdList);
	void Signal() const;
	void Flush();

	ID3D12CommandQueue* Get() const;
	ID3D12Fence* GetFence() const;
	UINT64& GetCurrFence();

private:
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> _cmdQueue = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Fence> _fence = nullptr;
	UINT64 _currFence = 0u;
};