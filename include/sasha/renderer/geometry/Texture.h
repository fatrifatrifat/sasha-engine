#pragma once
#include "../../utility/d3dUtil.h"
#include "../../../Include/sasha/sasha.h"
#include <ranges>

struct Texture
{
	Texture(ID3D12Device* device, CommandList& cmdList,const std::string& name, const std::wstring filename)
		: _name(name)
		, _filename(filename)
	{
		DirectX::ScratchImage image;
		ThrowIfFailed(DirectX::LoadFromWICFile(_filename.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image));

		DirectX::ScratchImage mipChain;
		ThrowIfFailed(DirectX::GenerateMipMaps(*image.GetImages(), DirectX::TEX_FILTER_BOX, 0, mipChain));

		const auto& chainBase = *mipChain.GetImages();
		D3D12_RESOURCE_DESC texDesc{};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Width = (UINT)chainBase.width;
		texDesc.Height = (UINT)chainBase.height;
		texDesc.DepthOrArraySize = 1;
		texDesc.MipLevels = (UINT16)mipChain.GetImageCount();
		texDesc.Format = chainBase.format;
		texDesc.SampleDesc.Count = 1;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		const CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(device->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&_resource)
		));

		auto subresourData = std::ranges::views::iota(0, (int)mipChain.GetImageCount()) |
			std::ranges::views::transform([&](int i) {
			const auto img = mipChain.GetImage(i, 0, 0);
			return D3D12_SUBRESOURCE_DATA{
				.pData = img->pixels,
				.RowPitch = (LONG_PTR)img->rowPitch,
				.SlicePitch = (LONG_PTR)img->slicePitch,
			};
				}) |
			std::ranges::to<std::vector>();

		const auto uploadBufferSize = GetRequiredIntermediateSize(
			_resource.Get(), 0, (UINT)subresourData.size()
		);
		const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

		const CD3DX12_HEAP_PROPERTIES heapPropUpload(D3D12_HEAP_TYPE_UPLOAD);
		ThrowIfFailed(device->CreateCommittedResource(
			&heapPropUpload,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&_uploadBuffer)
		));

		UpdateSubresources(
			cmdList.Get(),
			_resource.Get(),
			_uploadBuffer.Get(),
			0, 0,
			(UINT)subresourData.size(),
			subresourData.data()
		);

		cmdList.ChangeResourceState(_resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	std::string _name;
	std::wstring _filename;

	Microsoft::WRL::ComPtr<ID3D12Resource> _resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> _uploadBuffer = nullptr;
};
