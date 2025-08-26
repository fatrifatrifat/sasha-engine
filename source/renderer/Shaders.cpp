#include "../../include/sasha/renderer/Shaders.h"

std::filesystem::path Shader::_shaderFolderPath = std::filesystem::current_path() / ".." / "shaders";

Shader::Shader(std::string_view filename)
{
	auto file = _shaderFolderPath / filename;
	ThrowIfFailed(D3DReadFileToBlob(file.c_str(), &_shader));
}

D3D12_SHADER_BYTECODE Shader::GetByteCode() const noexcept
{
	return { _shader->GetBufferPointer(), _shader->GetBufferSize() };
}