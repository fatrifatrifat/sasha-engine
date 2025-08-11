#pragma once
#include "../utility/d3dUtil.h"
#include "geometry/Mesh.h"
#include "geometry/Material.h"

struct FrameResource
{
	FrameResource(ID3D12Device* device, UINT passCount = 1u, UINT cbCount = 1u, UINT matCount = 1u);
	FrameResource(const FrameResource&) = delete;
	FrameResource& operator=(const FrameResource&) = delete;
	~FrameResource() = default;

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _cmdAlloc;
	std::unique_ptr<d3dUtil::UploadBuffer<PassBuffer>> _pass = nullptr;
	std::unique_ptr<d3dUtil::UploadBuffer<ConstantBuffer>> _cb = nullptr;
	std::unique_ptr<d3dUtil::UploadBuffer<MaterialConstant>> _mat = nullptr;
	UINT64 _fence = 0u;
};
