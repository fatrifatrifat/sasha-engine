#pragma once
#include "../../utility/d3dUtil.h"
#include "../core/Device.h"
#include "../core/CommandList.h"
#include <filesystem>
#include <ranges>

struct Texture
{
	Texture(ID3D12Device* device, CommandList& cmdList, const std::string& name, std::string_view filename);

	static std::vector<CD3DX12_STATIC_SAMPLER_DESC> GetStaticSampler();

	std::string _name;

	Microsoft::WRL::ComPtr<ID3D12Resource> _resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> _uploadBuffer = nullptr;

	static std::filesystem::path _texPath;
};