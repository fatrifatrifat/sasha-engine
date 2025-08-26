#pragma once
#include "../utility/d3dUtil.h"
#include <filesystem>
#include "pipeline/PSOKey.h"

class Shader
{
public:
	Shader(std::string_view filename);

	D3D12_SHADER_BYTECODE GetByteCode() const noexcept;

private:
	Microsoft::WRL::ComPtr<ID3DBlob> _shader;

	static std::filesystem::path _shaderFolderPath;
};