#include "../../../include/sasha/renderer/core/Device.h"

using Microsoft::WRL::ComPtr;

Device::Device()
{
	// Enabling the debugging layer so we can more easily debug if something goes wrong
	#if defined (DEBUG) || defined (_DEBUG)
	{
		ComPtr<ID3D12Debug> debugLayer;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)));
		debugLayer->EnableDebugLayer();
	}
	#endif

	// Creating the factory for DXGI objects like SwapChain
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&_factory)));

	// Creating the device, basically the interface that let's you create of interfaces specific to d3d
	ThrowIfFailed(D3D12CreateDevice(
		nullptr,
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&_device)
	));

	// Checking the MSAA4x quality level
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualityLevels{};
	qualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	qualityLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	qualityLevels.NumQualityLevels = 0;
	qualityLevels.SampleCount = 4;
	ThrowIfFailed(_device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&qualityLevels,
		sizeof(qualityLevels)
	));
	assert(qualityLevels.NumQualityLevels > 0);
}

const ID3D12Device* Device::Get() const
{
	return _device.Get();
}

ID3D12Device* Device::Get()
{
	return _device.Get();
}

const IDXGIFactory4* Device::GetFactory() const
{
	return _factory.Get();
}

IDXGIFactory4* Device::GetFactory()
{
	return _factory.Get();
}
