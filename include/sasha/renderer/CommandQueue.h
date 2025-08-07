#pragma once
#include "../utility/d3dIncludes.h"

class CommandQueue
{
public:
	CommandQueue(ID3D12Device* device,
		D3D12_COMMAND_QUEUE_FLAGS queueFlags = D3D12_COMMAND_QUEUE_FLAG_NONE,
		D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		D3D12_FENCE_FLAGS fenceFlags = D3D12_FENCE_FLAG_NONE);

	void ExecuteCmdList(ID3D12GraphicsCommandList* cmdList) const;
	void Signal() const;
	void Flush() const;

	ID3D12CommandQueue* Get() const noexcept;
	ID3D12Fence* GetFence() const noexcept;
	UINT64& GetCurrFence() noexcept;

private:
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> _cmdQueue = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Fence> _fence = nullptr;
	mutable UINT64 _currFence = 0u;
};