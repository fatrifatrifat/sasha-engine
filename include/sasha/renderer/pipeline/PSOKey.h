#pragma once
#include "../../utility/d3dIncludes.h"
#include "GraphicsPipelineState.h"

namespace hashing
{
	inline uint64_t fnv1a64(const void* data, size_t size) noexcept
	{
		const uint8_t* byte = static_cast<const uint8_t*>(data);
		uint64_t hash = 1469598103934665603ull;
		for (size_t i = 0; i < size; i++)
		{
			hash ^= byte[i];
			hash *= 1099511628211ull;
		}
		return hash;
	}

	// From the boost library
	inline void hash_combine(uint64_t& seed, uint64_t value) noexcept
	{
		seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
	}

	inline uint64_t hash_shader_bytecode(const D3D12_SHADER_BYTECODE& bc) noexcept
	{
		if (!bc.pShaderBytecode || bc.BytecodeLength == 0) return 0ull;
		return fnv1a64(bc.pShaderBytecode, bc.BytecodeLength);
	}

	inline uint64_t hash_input_layout(const std::vector<D3D12_INPUT_ELEMENT_DESC>& layout) noexcept
	{
		uint64_t seed = 0ull;
		for (const auto& e : layout)
		{
			if (e.SemanticName)
				seed ^= fnv1a64(e.SemanticName, std::strlen(e.SemanticName));
			hash_combine(seed, e.SemanticIndex);
			hash_combine(seed, static_cast<uint64_t>(e.Format));
			hash_combine(seed, e.InputSlot);
			hash_combine(seed, e.AlignedByteOffset);
			hash_combine(seed, static_cast<uint64_t>(e.InputSlotClass));
			hash_combine(seed, e.InstanceDataStepRate);
		}
		return seed;
	}

	inline uint64_t hash_rasterizer(const D3D12_RASTERIZER_DESC& rasterizer) noexcept
	{
		uint64_t seed = 0ull;
		hash_combine(seed, rasterizer.AntialiasedLineEnable);
		hash_combine(seed, static_cast<uint64_t>(rasterizer.ConservativeRaster));
		hash_combine(seed, static_cast<uint64_t>(rasterizer.CullMode));
		hash_combine(seed, rasterizer.DepthBias);
		hash_combine(seed, rasterizer.DepthBiasClamp);
		hash_combine(seed, rasterizer.DepthClipEnable);
		hash_combine(seed, static_cast<uint64_t>(rasterizer.FillMode));
		hash_combine(seed, rasterizer.ForcedSampleCount);
		hash_combine(seed, rasterizer.FrontCounterClockwise);
		hash_combine(seed, rasterizer.MultisampleEnable);
		hash_combine(seed, rasterizer.SlopeScaledDepthBias);
		return seed;
	}

	inline uint64_t hash_blend(const D3D12_BLEND_DESC& blend, UINT numRT) noexcept
	{
		uint64_t seed = 0ull;
		hash_combine(seed, blend.AlphaToCoverageEnable);
		hash_combine(seed, blend.IndependentBlendEnable);
		for (size_t i = 0; i < numRT; i++)
		{
			const auto& rt = blend.RenderTarget[i];
			hash_combine(seed, rt.BlendEnable);
			hash_combine(seed, static_cast<uint64_t>(rt.SrcBlend));
			hash_combine(seed, static_cast<uint64_t>(rt.DestBlend));
			hash_combine(seed, static_cast<uint64_t>(rt.BlendOp));
			hash_combine(seed, static_cast<uint64_t>(rt.SrcBlendAlpha));
			hash_combine(seed, static_cast<uint64_t>(rt.DestBlendAlpha));
			hash_combine(seed, static_cast<uint64_t>(rt.BlendOpAlpha));
			hash_combine(seed, static_cast<uint64_t>(rt.RenderTargetWriteMask));
		}
		return seed;
	}

	inline uint64_t hash_depth(const D3D12_DEPTH_STENCIL_DESC& depth) noexcept
	{
		uint64_t seed = 0ull;
		hash_combine(seed, depth.DepthEnable);
		hash_combine(seed, static_cast<uint64_t>(depth.DepthWriteMask));
		hash_combine(seed, static_cast<uint64_t>(depth.DepthFunc));
		hash_combine(seed, depth.StencilEnable);
		hash_combine(seed, depth.StencilReadMask);
		hash_combine(seed, depth.StencilWriteMask);

		auto hFace = [&](const D3D12_DEPTH_STENCILOP_DESC& f) 
			{
				uint64_t s = 0ull;
				hash_combine(s, static_cast<uint64_t>(f.StencilFailOp));
				hash_combine(s, static_cast<uint64_t>(f.StencilDepthFailOp));
				hash_combine(s, static_cast<uint64_t>(f.StencilPassOp));
				hash_combine(s, static_cast<uint64_t>(f.StencilFunc));
				return s;
			};
		hash_combine(seed, hFace(depth.FrontFace));
		hash_combine(seed, hFace(depth.BackFace));
		return seed;
	}

	inline uint64_t hash_rt_formats(const RenderTargetDesc& rt) noexcept
	{
		uint64_t seed = 0ull;
		for (UINT i = 0; i < rt._numRenderTargets && i < 8; ++i) {
			hash_combine(seed, static_cast<uint64_t>(rt._rtvFormats[i]));
		}
		hash_combine(seed, static_cast<uint64_t>(rt._dsvFormat));
		hash_combine(seed, static_cast<uint64_t>(rt._sampleDesc.Count));
		hash_combine(seed, static_cast<uint64_t>(rt._sampleDesc.Quality));
		return seed;
	}
}

struct PSOKey
{
	// All the hashes
	uint64_t _rootSignature = 0;
	uint64_t _vs = 0;
	uint64_t _ps = 0;
	uint64_t _inputLayout = 0;
	uint64_t _rasterizer = 0;
	uint64_t _blend = 0;
	uint64_t _depth = 0;
	uint64_t _rtFormats = 0;
	uint8_t  _topology = static_cast<uint8_t>(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

	bool operator==(const PSOKey& rhs)
	{
		return _rootSignature == rhs._rootSignature && _vs == rhs._vs && _ps == rhs._ps && _inputLayout == rhs._inputLayout && _rasterizer == rhs._rasterizer
			&& _blend == rhs._blend && _depth == rhs._depth && _rtFormats == rhs._rtFormats && _topology == rhs._topology;
	}

	static PSOKey From(const GraphicsPipelineRecipe& recipe, ID3D12RootSignature* rootSignature, const RenderTargetDesc& rt)
	{
		PSOKey key{};
		key._rootSignature = reinterpret_cast<uint64_t>(rootSignature);
		key._vs = hashing::hash_shader_bytecode(recipe._vs);
		key._ps = hashing::hash_shader_bytecode(recipe._ps);
		key._rasterizer = hashing::hash_rasterizer(recipe._rasterizerDesc);
		key._blend = hashing::hash_blend(recipe._blendDesc, rt._numRenderTargets);
		key._depth = hashing::hash_depth(recipe._depthStentilDesc);
		key._inputLayout = hashing::hash_input_layout(recipe._inputLayout);
		key._rtFormats = hashing::hash_rt_formats(rt);
		key._topology = static_cast<uint8_t>(recipe._topology);
		return key;
	}
};

struct PSOKeyHash
{
	size_t operator()(const PSOKey& key) const noexcept
	{
		uint64_t seed = 0ull;
		hashing::hash_combine(seed, key._rootSignature);
		hashing::hash_combine(seed, key._vs);
		hashing::hash_combine(seed, key._ps);
		hashing::hash_combine(seed, key._inputLayout);
		hashing::hash_combine(seed, key._rasterizer);
		hashing::hash_combine(seed, key._blend);
		hashing::hash_combine(seed, key._depth);
		hashing::hash_combine(seed, key._rtFormats);
		hashing::hash_combine(seed, key._topology);
		return static_cast<size_t>(seed);
	}
};
