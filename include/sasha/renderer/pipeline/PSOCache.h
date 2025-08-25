#pragma once
#include "../../utility/d3dIncludes.h"
#include "GraphicsPipelineState.h"
#include "PSOKey.h"

class PSOCache
{
public:
	explicit PSOCache(ID3D12Device* device);

	ID3D12PipelineState* GetOrCreate(
		ID3D12RootSignature* rootSignature,
		const GraphicsPipelineRecipe& recipe,
		const RenderTargetDesc& rtDesc,
		const wchar_t* debugName = nullptr);

	void Clear();

private:
	Microsoft::WRL::ComPtr<ID3D12Device> _device;
	std::unordered_map<PSOKey, Microsoft::WRL::ComPtr<ID3D12PipelineState>, PSOKeyHash> _cache;

private:
	Microsoft::WRL::ComPtr<ID3D12PipelineState> CreatePSO(
		ID3D12RootSignature* rootSignature,
		const GraphicsPipelineRecipe& recipe,
		const RenderTargetDesc& rtDesc,
		const wchar_t* debugName);
};
