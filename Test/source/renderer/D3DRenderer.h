#include "../utility/d3dUtil.h"
#include "Mesh.h"

using namespace Microsoft::WRL;
using namespace DirectX;

class D3DRenderer
{
public:
	D3DRenderer(HWND wh, int w, int h);
	~D3DRenderer();
	
	D3DRenderer(const D3DRenderer&) = delete;
	D3DRenderer& operator=(const D3DRenderer&) = delete;

	void d3dInit();
	void RenderFrame(Timer& t);
	void Update(Timer& t);
	
	void OnResize();
	float AspectRatio() const noexcept;
	void SetAppSize(float w, float h) noexcept;

private:
	void CreateDevice();
	void CreateCmdObjs();
	void CreateSwapChain();
	void CreateDescriptorHeaps();
	void CreateRTV();
	void CreateDSV();

	void BuildInputLayout();
	void BuildGeometry();
	void BuildCbvDescriptorHeap();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildPSO();
	
	void FlushQueue();

	ID3D12Resource* GetCurrBackBuffer();
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrBackBufferView();
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSView();

	void ExecuteCmdList();
	void ChangeResourceState(ID3D12Resource* resource,
		D3D12_RESOURCE_STATES prevState,
		D3D12_RESOURCE_STATES nextState,
		UINT numBarriers = 1) const noexcept; 

	void BeginFrame();
	void DrawFrame();
	void EndFrame();

private:
	int _appWidth;
	int _appHeight;
	HWND _wndHandle;

	ComPtr<ID3D12Device> _device;
	ComPtr<IDXGIFactory4> _factory;
	ComPtr<IDXGISwapChain> _swapChain;

	ComPtr<ID3D12CommandQueue> _cmdQueue;
	ComPtr<ID3D12CommandAllocator> _cmdAlloc;
	ComPtr<ID3D12GraphicsCommandList> _cmdList;

	UINT _currFence = 0u;
	ComPtr<ID3D12Fence> _fence;

	UINT _rtvDescriptorSize = 0u;
	UINT _dsvDescriptorSize = 0u;
	UINT _cbvDescriptorSize = 0u;

	ComPtr<ID3D12DescriptorHeap> _rtvHeap;
	ComPtr<ID3D12DescriptorHeap> _dsvHeap;

	static const UINT bufferCount = 2u;
	UINT _currBackBuffer = 0u;
	ComPtr<ID3D12Resource> _swapChainBuffer[bufferCount];
	ComPtr<ID3D12Resource> _depthStencilBuffer;

	D3D12_VIEWPORT _vp{};
	D3D12_RECT _scissor{};

	ComPtr<ID3DBlob> _vertexShader;
	ComPtr<ID3DBlob> _pixelShader;
	std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayoutDesc{};

	XMFLOAT4X4 _world = d3dUtil::Identity4x4();
	XMFLOAT4X4 _view = d3dUtil::Identity4x4();
	XMFLOAT4X4 _proj = d3dUtil::Identity4x4();

	std::unique_ptr<MeshGeometry> _object;
	
	std::unique_ptr<d3dUtil::UploadBuffer<ConstantBuffer>> _constantBuffer;
	ComPtr<ID3D12DescriptorHeap> _cbvHeap;

	ComPtr<ID3D12PipelineState> _pso;
	ComPtr<ID3D12RootSignature> _rootSignature;
};