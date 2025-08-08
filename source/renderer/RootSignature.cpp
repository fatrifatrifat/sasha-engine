#include "../../include/sasha/renderer/RootSignature.h"

Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignature::Build(ID3D12Device* device, D3D12_ROOT_SIGNATURE_FLAGS flags)
{
	// Descriptor for the root signature
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc((UINT)_slotParameters.size(), _slotParameters.data(),
		0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	const CD3DX12_ROOT_SIGNATURE_DESC rootDesc(
		(UINT)_slotParameters.size(),
		_slotParameters.data(),
		0u, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(
		&rootDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),
		errorBlob.GetAddressOf()
	);

	if (errorBlob)
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	ThrowIfFailed(hr);

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	ThrowIfFailed(device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(rootSignature.GetAddressOf())
	));

	return rootSignature;
}

void RootSignature::AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE type, UINT numDesc, UINT shaderReg, UINT regSpace) noexcept
{
	auto range = std::make_unique<CD3DX12_DESCRIPTOR_RANGE>();
	range->Init(type, numDesc, shaderReg, regSpace);

	CD3DX12_ROOT_PARAMETER param;
	param.InitAsDescriptorTable(1, range.get());
	_slotParameters.push_back(param);

	_descriptorRanges.push_back(std::move(range));
}

void RootSignature::AddRootDescriptor() noexcept
{
}

void RootSignature::AddCBV(UINT shaderReg, UINT regSpace) noexcept
{
	CD3DX12_ROOT_PARAMETER param;
	param.InitAsConstantBufferView(shaderReg, regSpace);
	_slotParameters.push_back(param);
}

void RootSignature::AddRootConstant() noexcept
{
}
