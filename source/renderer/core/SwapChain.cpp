#include "../../../include/sasha/renderer/core/SwapChain.h"

SwapChain::SwapChain(HWND wndHandle, Device* device, CommandQueue* cmdQueue, UINT h, UINT w)
	: _appHeight(h)
	, _appWidth(w)
{
	// Creating the SwapChain object to be able to access and swap between render targets that we will show as frames
	DXGI_SWAP_CHAIN_DESC scDesc;
	scDesc.BufferDesc.Height = _appHeight;
	scDesc.BufferDesc.Width = _appWidth;
	scDesc.BufferDesc.RefreshRate.Numerator = 60;
	scDesc.BufferDesc.RefreshRate.Denominator = 1;
	scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	scDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	scDesc.SampleDesc.Count = 1;
	scDesc.SampleDesc.Quality = 0;

	scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scDesc.BufferCount = _bufferCount;
	scDesc.OutputWindow = wndHandle;
	scDesc.Windowed = true;
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	scDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ThrowIfFailed(device->GetFactory()->CreateSwapChain(
		cmdQueue->Get(),
		&scDesc,
		_swapChain.GetAddressOf()
	));
}

void SwapChain::Present(UINT interval, UINT flags)
{
	_swapChain->Present(interval, flags);
	_currBackBuffer = (_currBackBuffer + 1) % _bufferCount;
}

void SwapChain::OnResize(Device* device, CommandList* cmdList, const DescriptorHeap& rtvHeap, const DescriptorHeap& dsvHeap)
{
	for (int i = 0; i < _bufferCount; i++)
		_swapChainBuffer[i].Reset();
	_depthStencilBuffer.Reset();

	ThrowIfFailed(_swapChain->ResizeBuffers(_bufferCount,
		_appWidth, _appHeight,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	));

	_currBackBuffer = 0u;

	CreateRTV(device, rtvHeap);
	CreateDSV(device, cmdList, dsvHeap);
}

ID3D12Resource* SwapChain::GetCurrBackBuffer()
{
	return _swapChainBuffer[_currBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::GetCurrBackBufferView(const DescriptorHeap& rtvHeap)
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		rtvHeap.GetCPUStart(),
		_currBackBuffer,
		rtvHeap.GetSize()
	);
}

D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::GetDSView(const DescriptorHeap& dsvHeap)
{
	return dsvHeap.GetCPUStart();
}

D3D12_VIEWPORT* SwapChain::GetViewport()
{
	return &_vp;
}

D3D12_RECT* SwapChain::GetRect()
{
	return &_scissor;
}

void SwapChain::CreateRTV(Device* device, const DescriptorHeap& rtvHeap)
{
	// Creating a rtv with every buffer held by the SwapChain
	for (int i = 0; i < _bufferCount; i++)
	{
		ThrowIfFailed(_swapChain->GetBuffer(i, IID_PPV_ARGS(&_swapChainBuffer[i])));
		device->Get()->CreateRenderTargetView(_swapChainBuffer[i].Get(), nullptr, rtvHeap.GetCPUStart(i));
	}
}

void SwapChain::CreateDSV(Device* device, CommandList* cmdList, const DescriptorHeap& dsvHeap)
{
	// Creating the depth stencil view for the depth stencil buffer
	const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
	const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC
		::Tex2D(
			DXGI_FORMAT_D32_FLOAT,
			_appWidth, _appHeight,
			1, 0, 1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		);

	D3D12_CLEAR_VALUE clearVal{};
	clearVal.DepthStencil = { 1.f, 0 };
	clearVal.Format = DXGI_FORMAT_D32_FLOAT;

	ThrowIfFailed(device->Get()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&clearVal,
		IID_PPV_ARGS(_depthStencilBuffer.GetAddressOf())
	));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Texture2D.MipSlice = 0;
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	device->Get()->CreateDepthStencilView(_depthStencilBuffer.Get(), &dsvDesc, dsvHeap.GetCPUStart());

	cmdList->ChangeResourceState(_depthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	// Initialize the viewport and scissors rectangle
	_vp.MaxDepth = 1.f;
	_vp.MinDepth = 0.f;
	_vp.Height = static_cast<float>(_appHeight);
	_vp.Width = static_cast<float>(_appWidth);
	_vp.TopLeftX = 0.f;
	_vp.TopLeftY = 0.f;

	_scissor = { 0, 0, static_cast<long>(_appWidth), static_cast<long>(_appHeight) };
}