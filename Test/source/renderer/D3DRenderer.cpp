#include "D3DRenderer.h"
#include <filesystem>

D3DRenderer::D3DRenderer(HWND wh, int w, int h)
	: _wndHandle(wh)
	, _appWidth(w)
	, _appHeight(h)
{}

D3DRenderer::~D3DRenderer()
{
	if (_device)
		FlushQueue();
}

void D3DRenderer::d3dInit()
{
	CreateDevice();
	CreateCmdObjs();
	CreateSwapChain();
	CreateDescriptorHeaps();
	OnResize();

	ThrowIfFailed(_cmdList->Reset(_cmdAlloc.Get(), nullptr));

	BuildCbvDescriptorHeap();
	BuildConstantBuffers();
	BuildRootSignature();
	BuildInputLayout();
	BuildGeometry();
	BuildPSO();

	ExecuteCmdList();
	FlushQueue();
}

void D3DRenderer::RenderFrame(Timer& t)
{
	BeginFrame();
	DrawFrame();
	EndFrame();
}

void D3DRenderer::Update(Timer& t)
{
	ConstantBuffer cb;

	XMVECTOR pos = XMVectorSet(0.f, 0.f, -5.f, 1.f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&_view, view);

	XMMATRIX world = XMLoadFloat4x4(&_world);
	world = world * XMMatrixRotationY(t.TotalTime());

	XMMATRIX proj = XMLoadFloat4x4(&_proj);
	XMMATRIX worldViewProj = world * view * proj;

	XMStoreFloat4x4(&cb.WorldViewProj, XMMatrixTranspose(worldViewProj));

	_constantBuffer->CopyData(0, cb);
}

float D3DRenderer::AspectRatio() const noexcept
{
	return static_cast<float>(_appWidth) / _appHeight;
}

void D3DRenderer::CreateDevice()
{
	#if defined (DEBUG) || defined (_DEBUG)
	{
		ComPtr<ID3D12Debug> debugLayer;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)));
		debugLayer->EnableDebugLayer();
	}
	#endif

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&_factory)));

	ThrowIfFailed(D3D12CreateDevice(
		nullptr,
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&_device)
	));

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualityLevels{};
	qualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	qualityLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	qualityLevels.NumQualityLevels = 0;
	qualityLevels.SampleCount = 4;
	ThrowIfFailed(_device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&qualityLevels,
		sizeof(qualityLevels)
	));
	assert(qualityLevels.NumQualityLevels > 0);

}

void D3DRenderer::CreateCmdObjs()
{
	D3D12_COMMAND_QUEUE_DESC cqDesc{};
	cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(_device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&_cmdQueue)));

	ThrowIfFailed(_device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(_cmdAlloc.GetAddressOf())
	));

	ThrowIfFailed(_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		_cmdAlloc.Get(),
		nullptr,
		IID_PPV_ARGS(_cmdList.GetAddressOf())
	));

	ThrowIfFailed(_cmdList->Close());

	ThrowIfFailed(_device->CreateFence(
		0u, 
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&_fence)
	));
}

void D3DRenderer::CreateSwapChain()
{
	DXGI_SWAP_CHAIN_DESC scDesc;
	scDesc.BufferDesc.Height = _appHeight;
	scDesc.BufferDesc.Width = _appWidth;
	scDesc.BufferDesc.RefreshRate.Numerator = 60;
	scDesc.BufferDesc.RefreshRate.Denominator = 1;
	scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	scDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	scDesc.SampleDesc.Count = 1;
	scDesc.SampleDesc.Quality = 0;

	scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scDesc.BufferCount = bufferCount;
	scDesc.OutputWindow = _wndHandle;
	scDesc.Windowed = true;
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	scDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ThrowIfFailed(_factory->CreateSwapChain(
		_cmdQueue.Get(),
		&scDesc,
		_swapChain.GetAddressOf()
	));
}

void D3DRenderer::CreateDescriptorHeaps()
{
	_rtvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	_dsvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	_cbvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
	rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvDesc.NumDescriptors = bufferCount;
	rtvDesc.NodeMask = 0;
	rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	_device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&_rtvHeap));

	D3D12_DESCRIPTOR_HEAP_DESC dsvDesc{};
	rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvDesc.NumDescriptors = 1u;
	rtvDesc.NodeMask = 0;
	rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	_device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&_dsvHeap));
}

void D3DRenderer::CreateRTV()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvBegHandle(_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (int i = 0; i < bufferCount; i++)
	{
		ThrowIfFailed(_swapChain->GetBuffer(i, IID_PPV_ARGS(&_swapChainBuffer[i])));
		_device->CreateRenderTargetView(_swapChainBuffer[i].Get(), nullptr, rtvBegHandle);
		rtvBegHandle.Offset(_rtvDescriptorSize);
	}
}

void D3DRenderer::CreateDSV()
{
	const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
	const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC
		::Tex2D(
			DXGI_FORMAT_D32_FLOAT,
			_appWidth, _appHeight,
			1, 0, 1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		);

	D3D12_CLEAR_VALUE clearVal{};
	clearVal.DepthStencil = { 1.f, 0 };
	clearVal.Format = DXGI_FORMAT_D32_FLOAT;

	ThrowIfFailed(_device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&clearVal,
		IID_PPV_ARGS(_depthStencilBuffer.GetAddressOf())
	));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Texture2D.MipSlice = 0;
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	_device->CreateDepthStencilView(_depthStencilBuffer.Get(), &dsvDesc, GetDSView());

	ChangeResourceState(_depthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	_vp.MaxDepth = 1.f;
	_vp.MinDepth = 0.f;
	_vp.Height = static_cast<float>(_appHeight);
	_vp.Width = static_cast<float>(_appWidth);
	_vp.TopLeftX = 0.f;
	_vp.TopLeftY = 0.f;

	_scissor = { 0, 0, _appWidth, _appHeight };

	ExecuteCmdList();
	FlushQueue();
}

void D3DRenderer::BuildInputLayout()
{
	std::filesystem::path shaderPath1 = std::filesystem::current_path() / "shaders" / "defaultVS.cso";
	std::filesystem::path shaderPath2 = std::filesystem::current_path() / "shaders" / "defaultPS.cso";

	ThrowIfFailed(D3DReadFileToBlob(shaderPath1.c_str(), &_vertexShader));
	ThrowIfFailed(D3DReadFileToBlob(shaderPath2.c_str(), &_pixelShader));

	_inputLayoutDesc =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void D3DRenderer::BuildGeometry()
{
	// Building vertex buffer
	std::array<Vertex, 8> vertices =
	{
		Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
		Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
	};

	std::array<std::uint16_t, 36> indices =
	{
		// front face
		0, 1, 2,
		0, 2, 3,

		// back face
		4, 6, 5,
		4, 7, 6,

		// left face
		4, 5, 1,
		4, 1, 0,

		// right face
		3, 2, 6,
		3, 6, 7,

		// top face
		1, 5, 6,
		1, 6, 2,

		// bottom face
		4, 0, 3,
		4, 3, 7
	};

	_object = std::make_unique<MeshGeometry>(_device.Get(), _cmdList.Get(), vertices, indices);
	_object->Name = "boxGeo";

	SubmeshGeometry submesh;
	submesh._indexCount = (UINT)indices.size();
	submesh._startIndexLocation = 0;
	submesh._baseVertexLocation = 0;

	_object->_subGeometry["box"] = submesh;
}

void D3DRenderer::BuildCbvDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc{};
	cbvHeapDesc.NumDescriptors = 1;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;

	ThrowIfFailed(_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&_cbvHeap)));
}

void D3DRenderer::BuildConstantBuffers()
{
	_constantBuffer = std::make_unique<d3dUtil::UploadBuffer<ConstantBuffer>>(_device.Get(), 1, true);

	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = _constantBuffer->GetResource()->GetGPUVirtualAddress();
	UINT cbSize = d3dUtil::CalcConstantBufferSize(sizeof(ConstantBuffer));

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes = cbSize;

	_device->CreateConstantBufferView(&cbvDesc, _cbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void D3DRenderer::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER slotRootParameter[1];

	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),
		errorBlob.GetAddressOf()
	);

	if (errorBlob)
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	ThrowIfFailed(hr);
	
	ThrowIfFailed(_device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&_rootSignature)
	));
}

void D3DRenderer::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { 
		_inputLayoutDesc.data(),
		(UINT)_inputLayoutDesc.size()
	};
	psoDesc.pRootSignature = _rootSignature.Get();
	psoDesc.VS = { 
		reinterpret_cast<BYTE*>(_vertexShader->GetBufferPointer()),
		_vertexShader->GetBufferSize()
	};
	psoDesc.PS = {
		reinterpret_cast<BYTE*>(_pixelShader->GetBufferPointer()),
		_pixelShader->GetBufferSize()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	ThrowIfFailed(_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_pso)));
}

void D3DRenderer::FlushQueue()
{
	_currFence++;
	_cmdQueue->Signal(_fence.Get(), _currFence);

	if (_fence->GetCompletedValue() < _currFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		_fence->SetEventOnCompletion(_currFence, eventHandle);
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

void D3DRenderer::OnResize()
{
	FlushQueue();
	_cmdList->Reset(_cmdAlloc.Get(), nullptr);

	for (int i = 0; i < bufferCount; i++)
		_swapChainBuffer[i].Reset();
	_depthStencilBuffer.Reset();

	ThrowIfFailed(_swapChain->ResizeBuffers(bufferCount,
		_appWidth, _appHeight,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	));

	_currBackBuffer = 0u;

	CreateRTV();
	CreateDSV();

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * 3.14159265f, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&_proj, P);
}

ID3D12Resource* D3DRenderer::GetCurrBackBuffer()
{
	return _swapChainBuffer[_currBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DRenderer::GetCurrBackBufferView()
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
		_currBackBuffer,
		_rtvDescriptorSize
	);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DRenderer::GetDSView()
{
	return _dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3DRenderer::ExecuteCmdList()
{
	ThrowIfFailed(_cmdList->Close());
	ID3D12CommandList* cmdLists[] = { _cmdList.Get() };
	_cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
}

void D3DRenderer::ChangeResourceState(
	ID3D12Resource* resource,
	D3D12_RESOURCE_STATES prevState,
	D3D12_RESOURCE_STATES nextState,
	UINT numBarriers) const noexcept
{
	const CD3DX12_RESOURCE_BARRIER barrier(
		CD3DX12_RESOURCE_BARRIER
		::Transition(
			resource,
			prevState,
			nextState
		)
	);

	_cmdList->ResourceBarrier(numBarriers, &barrier);
}

void D3DRenderer::BeginFrame()
{
	ThrowIfFailed(_cmdAlloc->Reset());
	ThrowIfFailed(_cmdList->Reset(_cmdAlloc.Get(), _pso.Get()));

	ChangeResourceState(GetCurrBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void D3DRenderer::DrawFrame()
{
	_cmdList->RSSetViewports(1, &_vp);
	_cmdList->RSSetScissorRects(1, &_scissor);

	_cmdList->ClearRenderTargetView(GetCurrBackBufferView(), Colors::SteelBlue, 0, nullptr);
	_cmdList->ClearDepthStencilView(GetDSView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);

	auto currBackBufferView = GetCurrBackBufferView();
	auto dsv = GetDSView();
	_cmdList->OMSetRenderTargets(1, &currBackBufferView, true, &dsv);

	ID3D12DescriptorHeap* descriptorHeaps[] = { _cbvHeap.Get() };
	_cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	_cmdList->SetGraphicsRootSignature(_rootSignature.Get());

	auto vbv = _object->VertexBufferView();
	auto ibv = _object->IndexBufferView();
	_cmdList->IASetVertexBuffers(0, 1, &vbv);
	_cmdList->IASetIndexBuffer(&ibv);
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	_cmdList->SetGraphicsRootDescriptorTable(0, _cbvHeap->GetGPUDescriptorHandleForHeapStart());
	//_cmdList->SetPipelineState(_pso.Get());

	_cmdList->DrawIndexedInstanced(_object->_subGeometry["box"]._indexCount, 1u, 0u, 0u, 0u);
}

void D3DRenderer::EndFrame()
{
	ChangeResourceState(GetCurrBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	ExecuteCmdList();
	_swapChain->Present(0, 0);
	_currBackBuffer = (_currBackBuffer + 1) % bufferCount;

	FlushQueue();
}
