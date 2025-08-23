#pragma once
#include "../../utility/d3dIncludes.h"

class Device
{
public:
	Device();
	~Device() = default;

	Device(const Device&) = delete;
	Device& operator=(const Device&) = delete;

	const ID3D12Device* Get() const;
	ID3D12Device* Get();

	const IDXGIFactory4* GetFactory() const;
	IDXGIFactory4* GetFactory();

private:
	Microsoft::WRL::ComPtr<ID3D12Device> _device;
	Microsoft::WRL::ComPtr<IDXGIFactory4> _factory;
};
