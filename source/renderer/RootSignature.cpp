#include "../../include/sasha/renderer/RootSignature.h"

Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignature::Build(
	ID3D12Device* device,
	std::vector<CD3DX12_STATIC_SAMPLER_DESC> staticSamplers,
	D3D12_ROOT_SIGNATURE_FLAGS flags)
{
	// Descriptor for the root signature
	const CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		static_cast<UINT>(_slotParameters.size()),
		_slotParameters.data(),
		static_cast<UINT>(staticSamplers.size()),
		staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(
		&rootSigDesc,
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


void RootSignature::AddCBV(UINT shaderReg, UINT regSpace) noexcept
{
	CD3DX12_ROOT_PARAMETER param;
	param.InitAsConstantBufferView(shaderReg, regSpace);
	_slotParameters.push_back(param);
}

void RootSignature::AddRootConstant() noexcept
{
}
