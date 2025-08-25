#include "../../../include/sasha/renderer/pipeline/PSOCache.h"

using Microsoft::WRL::ComPtr;

static void validate_inputs(ID3D12RootSignature* rs, const GraphicsPipelineRecipe& recipe, const RenderTargetDesc& rt)
{
	assert(rs);
	assert(rt._sampleDesc.Count >= 1);
	if (rt._numRenderTargets > 0)
		assert(recipe._ps.pShaderBytecode && recipe._ps.BytecodeLength);
}

PSOCache::PSOCache(ID3D12Device* device)
	: _device(device)
{}

ID3D12PipelineState* PSOCache::GetOrCreate(
	ID3D12RootSignature* rootSignature,
	const GraphicsPipelineRecipe& recipe,
	const RenderTargetDesc& rtDesc,
	const wchar_t* debugName)
{
	PSOKey key = PSOKey::From(recipe, rootSignature, rtDesc);
	if (_cache.find(key) != _cache.end())
		return _cache.at(key).Get();

	ComPtr<ID3D12PipelineState> pso = CreatePSO(rootSignature, recipe, rtDesc, debugName);
	auto [pos, inserted] = _cache.emplace(key, std::move(pso));
	return pos->second.Get();
}

void PSOCache::Clear()
{
	_cache.clear();
}

ComPtr<ID3D12PipelineState> PSOCache::CreatePSO(
	ID3D12RootSignature* rootSignature,
	const GraphicsPipelineRecipe& recipe,
	const RenderTargetDesc& rtDesc,
	const wchar_t* debugName)
{
	validate_inputs(rootSignature, recipe, rtDesc);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	ZeroMemory(&desc, sizeof(desc));

	desc.VS = recipe._vs;
	desc.PS = recipe._ps;

	desc.InputLayout = { recipe._inputLayout.data(), static_cast<UINT>(recipe._inputLayout.size()) };

	desc.RasterizerState = recipe._rasterizerDesc;
	desc.BlendState = recipe._blendDesc;
	desc.DepthStencilState = recipe._depthStentilDesc;
	desc.SampleMask = recipe._sampleMask;
	desc.PrimitiveTopologyType = recipe._topology;

	desc.NumRenderTargets = rtDesc._numRenderTargets;
	for (UINT i = 0; i < rtDesc._numRenderTargets && i < 8; i++)
		desc.RTVFormats[i] = rtDesc._rtvFormats[i];
	desc.DSVFormat = rtDesc._dsvFormat;
	desc.SampleDesc = rtDesc._sampleDesc;

	desc.pRootSignature = rootSignature;

	ComPtr<ID3D12PipelineState> pso;
	ThrowIfFailed(_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso)));
	if (debugName && pso)
		pso->SetName(debugName);
	return pso;
}