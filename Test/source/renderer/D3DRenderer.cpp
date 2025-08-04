#include "D3DRenderer.h"
#include <filesystem>
#include <thread>

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

	BuildRootSignature();
	BuildInputLayout();
	BuildGeometry();
	BuildScene();
	BuildFrameResources();
	BuildCbvDescriptorHeap();
	BuildConstantBuffers();
	BuildPSO();

	ExecuteCmdList();
	FlushQueue();
}

void D3DRenderer::SetInputs(Keyboard* kb, Mouse* m) noexcept
{
	_kbd = kb;
	_mouse = m;
}

void D3DRenderer::RenderFrame(Timer& t)
{
	BeginFrame();
	DrawFrame();
	EndFrame();
}

void D3DRenderer::Update(Timer& t)
{
	if (_kbd->IsKeyPressed('W'))
		_cameraPos.z += _cameraSpeed * t.DeltaTime();
	else if (_kbd->IsKeyPressed('S'))
		_cameraPos.z -= _cameraSpeed * t.DeltaTime();
	else if (_kbd->IsKeyPressed('D'))
		_cameraPos.x += _cameraSpeed * t.DeltaTime();
	else if (_kbd->IsKeyPressed('A'))
		_cameraPos.x -= _cameraSpeed * t.DeltaTime();
	else if (_kbd->IsKeyPressed(VK_SPACE))
		_cameraPos.y += _cameraSpeed * t.DeltaTime();
	else if (_kbd->IsKeyPressed(VK_CONTROL))
		_cameraPos.y -= _cameraSpeed * t.DeltaTime();
	else if (_kbd->IsKeyPressed(VK_F2) && _kbd->WasKeyPressedThisFrame(VK_F2))
		_isWireFrame = !_isWireFrame;

	_frameResourceIndex = (_frameResourceIndex + 1) % _frameResourceCount;
	_currFrameResource = _frameResources[_frameResourceIndex].get();

	if (_currFrameResource->_fence != 0 && _fence->GetCompletedValue() < _currFrameResource->_fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		_fence->SetEventOnCompletion(_currFrameResource->_fence, eventHandle);
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	auto currObjCB = _currFrameResource->_cb.get();
	for (auto& e : _scene.GetRenderItems())
	{
		XMMATRIX world = XMLoadFloat4x4(&e->_world);

		ConstantBuffer cb;
		XMStoreFloat4x4(&cb.world, XMMatrixTranspose(world));

		currObjCB->CopyData(e->_cbObjIndex, cb);
	}

	XMVECTOR pos = XMLoadFloat4(&_cameraPos);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&_view, view);

	XMMATRIX proj = XMLoadFloat4x4(&_proj);

	XMMATRIX viewProj = view * proj;
	auto a = XMMatrixDeterminant(view);
	auto b = XMMatrixDeterminant(proj);
	auto c = XMMatrixDeterminant(viewProj);
	XMMATRIX invView = XMMatrixInverse(&a, view);
	XMMATRIX invProj = XMMatrixInverse(&b, proj);
	XMMATRIX invViewProj = XMMatrixInverse(&c, viewProj);

	XMStoreFloat4x4(&_mainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&_mainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&_mainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&_mainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&_mainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&_mainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	_mainPassCB.EyePosW = { _cameraPos.x, _cameraPos.y, _cameraPos.z };
	_mainPassCB.RenderTargetSize = XMFLOAT2((float)_appWidth, (float)_appHeight);
	_mainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / _appWidth, 1.0f / _appHeight);
	_mainPassCB.NearZ = 1.0f;
	_mainPassCB.FarZ = 1000.0f;
	_mainPassCB.TotalTime = t.TotalTime();
	_mainPassCB.DeltaTime = t.DeltaTime();

	_currFrameResource->_pass->CopyData(0, _mainPassCB);
}

float D3DRenderer::AspectRatio() const noexcept
{
	return static_cast<float>(_appWidth) / _appHeight;
}

void D3DRenderer::SetAppSize(int w, int h) noexcept
{
	_appHeight = h;
	_appWidth = w;	
}

void D3DRenderer::CreateDevice()
{
	// Enabling the debugging layer so we can more easily debug if something goes wrong
	#if defined (DEBUG) || defined (_DEBUG)
	{
		ComPtr<ID3D12Debug> debugLayer;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)));
		debugLayer->EnableDebugLayer();
	}
	#endif

	// Creating the factory for DXGI objects like SwapChain
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&_factory)));

	// Creating the device, basically the interface that let's you create of interfaces specific to d3d
	ThrowIfFailed(D3D12CreateDevice(
		nullptr,
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&_device)
	));

	// Checking the MSAA4x quality level
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
	// Creating the command queue which will contain the lists of command that was sent to the GPU
	D3D12_COMMAND_QUEUE_DESC cqDesc{};
	cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(_device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&_cmdQueue)));

	// Creating the command allocator which will let you allocate memory of command lists
	ThrowIfFailed(_device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(_cmdAlloc.GetAddressOf())
	));

	// Creating the command list which will contain the list of commands that will the sent to the command queue
	ThrowIfFailed(_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		_cmdAlloc.Get(),
		nullptr,
		IID_PPV_ARGS(_cmdList.GetAddressOf())
	));

	ThrowIfFailed(_cmdList->Close());

	// Creating a fence object so we can synchronize the CPU and GPU
	ThrowIfFailed(_device->CreateFence(
		0u, 
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&_fence)
	));
}

void D3DRenderer::CreateSwapChain()
{
	// Creating the SwapChain object to be able to access and swap between render targets that we will show as frames
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
	// Getting the size of a render target view descriptor (rtv), depth stencil view (dsv) and, constant buffer view, shader resource view and UAV
	_rtvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	_dsvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	_cbvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Creating descriptor heaps for the rtv and dsv which will contain rtvs and dsv descriptor that will be bind to the GPU pipeline
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
	// Creating a rtv with every buffer held by the SwapChain
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
	// Creating the depth stencil view for the depth stencil buffer
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

	// Initialize the viewport and scissors rectangle
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
	// Getting and compiling the shaders
	std::filesystem::path shaderPath1 = std::filesystem::current_path() / "shaders" / "defaultVS.cso";
	std::filesystem::path shaderPath2 = std::filesystem::current_path() / "shaders" / "defaultPS.cso";

	ThrowIfFailed(D3DReadFileToBlob(shaderPath1.c_str(), &_vertexShader));
	ThrowIfFailed(D3DReadFileToBlob(shaderPath2.c_str(), &_pixelShader));

	// Creating the input layout
	_inputLayoutDesc =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void D3DRenderer::BuildGeometry()
{
	// Concatenating every vertices in the same array as well as for the indices for more efficient draw calls with a technique called instancing
	GeometryGenerator g;
	auto geoSphere = g.CreateGeosphere(1.f, 3);
	auto box = g.CreateBox(1.f, 1.f, 1.f, 0);
	auto cylinder = g.CreateCylinder(0.5f, 0.3f, 3.f, 10, 10);
	auto grid = g.CreateGrid(160.f, 160.f, 250, 250);
	for (size_t i = 0; i < grid.Vertices.size(); i++)
	{
		auto& pos = grid.Vertices[i].Position;
		pos.y = 0.3f * (pos.z * sinf(0.1f * pos.x) + pos.x * cosf(0.1f * pos.z));
		grid.Vertices[i].Color= i % 2 ? XMFLOAT3(0.f, 0.f, 0.f) : XMFLOAT3(1.f, 1.f, 1.f);
	}
	_geoLib.AddGeometry("box", box, XMFLOAT4(Colors::LightPink));
	_geoLib.AddGeometry("sphere", geoSphere, XMFLOAT4(Colors::Pink));
	_geoLib.AddGeometry("cylinder", cylinder, XMFLOAT4(Colors::DeepPink));
	_geoLib.AddGeometry("grid", grid);

	// Once all are added:
	_geoLib.Upload(_device.Get(), _cmdList.Get());
}

void D3DRenderer::BuildScene()
{
	_scene.AddInstance("grid");

	_scene.BuildRenderItems(_geoLib);
}

void D3DRenderer::BuildFrameResources()
{
	// Build the Frame Resources
	for (int i = 0; i < _frameResourceCount; i++)
		_frameResources.push_back(std::make_unique<FrameResource>(_device.Get(), 1u, _scene.GetRenderItems().size()));
}

void D3DRenderer::BuildCbvDescriptorHeap()
{
	// Creating the constant buffer descriptor heap
	// Making a heap capable of holding 3n + 3 descriptors
	// 3n so each object can have their own frame resource on each frame resource
	// + 3 so each frame resource has access to it's global constant buffer that's non unique to each object
	UINT descriptorHeapCount = (_scene.GetRenderItems().size() + 1) * _frameResourceCount;
	// Getting the index at which the global constant buffers are, right after all the unique constant buffers
	_passCbvOffset = _frameResourceCount * _scene.GetRenderItems().size();

	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc{};
	cbvHeapDesc.NumDescriptors = descriptorHeapCount;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;

	ThrowIfFailed(_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&_cbvHeap)));
}

void D3DRenderer::BuildConstantBuffers()
{
	// Building the constant buffers at the place of each descriptor inside the heap created earlier
	// Getting the size of both cbs padded ceiled to a multiple of 256 for padding reasons on the shader and GPU side
	UINT cbSize = d3dUtil::CalcConstantBufferSize(sizeof(ConstantBuffer));
	UINT passSize = d3dUtil::CalcConstantBufferSize(sizeof(PassBuffer));

	for (UINT i = 0; i < _frameResourceCount; i++)
	{
		// Get the constant buffer from the current frame resource
		auto objCB = _frameResources[i]->_cb->GetResource();
		for (UINT j = 0; j < _scene.GetRenderItems().size(); j++)
		{
			// For each object, get the virtual address of the constant buffer and increment to access the jth object's cb
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objCB->GetGPUVirtualAddress();
			cbAddress += j * cbSize;
			
			// Get the index on the heap that contains the constant buffer for every object and get a handle to the right position of the constant buffer
			int heapIndex = i * _scene.GetRenderItems().size() + j;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(_cbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, _cbvDescriptorSize);

			// Create constant buffer view
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = cbSize;

			_device->CreateConstantBufferView(&cbvDesc, handle);
		}

		// Do something similar for the global constant buffers that are shared between objects in the same frame resource
		auto passCB = _frameResources[i]->_pass->GetResource();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		int heapIndex = _passCbvOffset + i;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(_cbvHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, _cbvDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passSize;

		_device->CreateConstantBufferView(&cbvDesc, handle);
	}
}

void D3DRenderer::BuildRootSignature()
{
	// This describes a slot for the constant buffers for the shaders
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	// This describes the descriptor range
	CD3DX12_DESCRIPTOR_RANGE cbvTable[2];
	cbvTable[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	cbvTable[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable[0]);
	slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable[1]);

	// Descriptor for the root signature
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter,
	 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// Serializes the root signature to create it later
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
		IID_PPV_ARGS(_rootSignature.GetAddressOf())
	));
}

void D3DRenderer::BuildPSO()
{
	// Building the Pipeline State Object to prepare the GPU to get parts of the pipeline in certain ways
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
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	ThrowIfFailed(_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_pso[0])));

	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

	ThrowIfFailed(_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_pso[1])));
}

void D3DRenderer::FlushQueue()
{
	// Synchronizes the CPU and the GPU to a certain command list
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
	// Called whenever the size of the app is changed
	// Also creates the render target view as well as the depth stencil view
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

	ThrowIfFailed(_currFrameResource->_cmdAlloc->Reset());
	if (_isWireFrame)
	{
		ThrowIfFailed(_cmdList->Reset(_currFrameResource->_cmdAlloc.Get(), _pso[0].Get()));
	}
	else
		ThrowIfFailed(_cmdList->Reset(_currFrameResource->_cmdAlloc.Get(), _pso[1].Get()));

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

	int passCbvIndex = _passCbvOffset + _frameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(_cbvHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, _cbvDescriptorSize);
	_cmdList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

	for (const auto& ri : _scene.GetRenderItems())
	{
		auto vbv = ri->_mesh->VertexBufferView();
		auto ibv = ri->_mesh->IndexBufferView();

		_cmdList->IASetVertexBuffers(0, 1, &vbv);
		_cmdList->IASetIndexBuffer(&ibv);
		_cmdList->IASetPrimitiveTopology(ri->_primitiveType);

		UINT cbvIndex = _frameResourceIndex * (UINT)_scene.GetRenderItems().size() + ri->_cbObjIndex;
		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(_cbvHeap->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, _cbvDescriptorSize);

		_cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);

		_cmdList->DrawIndexedInstanced(ri->_indexCount, 1u, ri->_startIndex, ri->_baseVertex, 0u);
	}
	
}

void D3DRenderer::EndFrame()
{
	ChangeResourceState(GetCurrBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	ExecuteCmdList();
	_swapChain->Present(0, 0);
	_currBackBuffer = (_currBackBuffer + 1) % bufferCount;

	_currFrameResource->_fence = ++_currFence;

	_cmdQueue->Signal(_fence.Get(), _currFence);
}