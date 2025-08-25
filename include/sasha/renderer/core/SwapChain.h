#pragma once
#include "../../utility/d3dIncludes.h"
#include "Device.h"
#include "CommandQueue.h"
#include "CommandList.h"
#include "../DescriptorHeap.h"

class SwapChain
{
public:
	SwapChain(HWND wndHandle, Device* device, CommandQueue* cmdQueue, UINT h, UINT w);
	~SwapChain() = default;

	SwapChain(const SwapChain&) = delete;
	SwapChain& operator=(const SwapChain&) = delete;

	void Present(UINT interval = 0, UINT flags = 0);
	void OnResize(Device* device, CommandList* cmdList, const DescriptorHeap& rtvHeap, const DescriptorHeap& dsvHeap);

	ID3D12Resource* GetCurrBackBuffer();
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrBackBufferView(const DescriptorHeap& rtvHeap);
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSView(const DescriptorHeap& dsvHeap);
	D3D12_VIEWPORT* GetViewport();
	D3D12_RECT* GetRect();
	DXGI_FORMAT GetRtFormat() const noexcept;
	DXGI_FORMAT GetDsvFormat() const noexcept;

private:
	void CreateRTV(Device* device, const DescriptorHeap& rtvHeap);
	void CreateDSV(Device* device, CommandList* cmdList, const DescriptorHeap& dsvHeap);

private:
	Microsoft::WRL::ComPtr<IDXGISwapChain> _swapChain;
	DXGI_FORMAT _rtFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT _dsvFormat = DXGI_FORMAT_D32_FLOAT;

	D3D12_VIEWPORT _vp{};
	D3D12_RECT _scissor{};

	static constexpr UINT _bufferCount = 2u;
	UINT _currBackBuffer = 0u;
	Microsoft::WRL::ComPtr<ID3D12Resource> _swapChainBuffer[_bufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> _depthStencilBuffer;

	UINT _appHeight;
	UINT _appWidth;
};
