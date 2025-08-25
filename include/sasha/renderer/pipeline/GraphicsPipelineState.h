#pragma once
#include "../../utility/d3dIncludes.h"

struct RenderTargetDesc
{
	UINT _numRenderTargets = 1u;
	std::array<DXGI_FORMAT, 8> _rtvFormats{};
	DXGI_FORMAT _dsvFormat = DXGI_FORMAT_D32_FLOAT;
	DXGI_SAMPLE_DESC _sampleDesc{ 1u, 0u };

	RenderTargetDesc()
	{
		_rtvFormats.fill(DXGI_FORMAT_UNKNOWN);
		_rtvFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	}
};

struct GraphicsPipelineRecipe
{
	D3D12_SHADER_BYTECODE _vs;
	D3D12_SHADER_BYTECODE _ps;

	std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayout;

	D3D12_RASTERIZER_DESC _rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	D3D12_BLEND_DESC _blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	D3D12_DEPTH_STENCIL_DESC _depthStentilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

	D3D12_PRIMITIVE_TOPOLOGY_TYPE _topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	UINT _sampleMask = UINT_MAX;

	static GraphicsPipelineRecipe MakeDefault(
		const std::vector<D3D12_INPUT_ELEMENT_DESC>& _layout,
		const D3D12_SHADER_BYTECODE& vs,
		const D3D12_SHADER_BYTECODE& ps)
	{
		GraphicsPipelineRecipe pso{};
		pso._inputLayout = _layout;
		pso._vs = vs;
		pso._ps = ps;
		return pso;
	}
};
