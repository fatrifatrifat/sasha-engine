#pragma once
#include "../../utility/d3dUtil.h"
#include "../../../Include/sasha/sasha.h"
#include <ranges>

struct Texture
{
	std::string _name;
	std::wstring _filename;

	Microsoft::WRL::ComPtr<ID3D12Resource> _resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> _uploadBuffer = nullptr;
};
