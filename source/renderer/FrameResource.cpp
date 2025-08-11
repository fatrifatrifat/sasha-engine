#include "../../include/sasha/renderer/FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT cbCount, UINT matCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(_cmdAlloc.GetAddressOf())
	));

	_pass = std::make_unique<d3dUtil::UploadBuffer<PassBuffer>>(device, passCount, true);
	_cb = std::make_unique<d3dUtil::UploadBuffer<ConstantBuffer>>(device, cbCount, true);
	_mat = std::make_unique<d3dUtil::UploadBuffer<MaterialConstant>>(device, matCount, true);
}