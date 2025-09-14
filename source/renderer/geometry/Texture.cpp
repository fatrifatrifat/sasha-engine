#include "../../../include/sasha/renderer/geometry/Texture.h"

using namespace DirectX;

std::filesystem::path Texture::_texPath = std::filesystem::current_path() / ".." / "assets" / "textures";

static ScratchImage LoadMipFromPath(const std::filesystem::path& p)
{
	ScratchImage image;
	ThrowIfFailed(LoadFromWICFile(p.wstring().c_str(), WIC_FLAGS_NONE, nullptr, image));

	ScratchImage mipChain;
	ThrowIfFailed(GenerateMipMaps(*image.GetImages(), TEX_FILTER_BOX, 0, mipChain));

	return mipChain;
}

Texture::Texture(ID3D12Device* device, CommandList& cmdList, const std::string& name, const std::filesystem::path& filename, bool path)
	: _name(name)
{
	DirectX::ScratchImage mipChain;
	if (!path)
	{
		auto file = _texPath / filename;
		mipChain = LoadMipFromPath(file);
	}
	else
	{
		mipChain = LoadMipFromPath(filename);
	}

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

	std::vector<D3D12_SUBRESOURCE_DATA> subresourceData;
	subresourceData.reserve(mipChain.GetImageCount());

	for (size_t i = 0; i < mipChain.GetImageCount(); ++i) 
	{
		const DirectX::Image* img = mipChain.GetImage(i, 0, 0);
		subresourceData.emplace_back(
			img->pixels,
			static_cast<LONG_PTR>(img->rowPitch),
			static_cast<LONG_PTR>(img->slicePitch)
		);
	}

	const auto uploadBufferSize = GetRequiredIntermediateSize(
		_resource.Get(), 0, (UINT)subresourceData.size()
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
		(UINT)subresourceData.size(),
		subresourceData.data()
	);

	cmdList.ChangeResourceState(_resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

std::vector<CD3DX12_STATIC_SAMPLER_DESC> Texture::GetStaticSampler()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	 // and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}
