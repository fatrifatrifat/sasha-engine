#pragma once
#include "../utility/d3dIncludes.h"

class RootSignature
{
public:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> Build(ID3D12Device* device, D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	void AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE type, UINT numDesc, UINT shaderReg, UINT regSpace = 0u) noexcept;
	void AddRootDescriptor() noexcept;
	void AddCBV(UINT shaderReg, UINT regSpace = 0u) noexcept;
	void AddRootConstant() noexcept;

private:
	std::vector<D3D12_ROOT_PARAMETER> _slotParameters;
	std::vector<std::unique_ptr<CD3DX12_DESCRIPTOR_RANGE>> _descriptorRanges;
};
