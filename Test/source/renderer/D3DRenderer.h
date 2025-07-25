#include "../utility/d3dUtil.h"

using namespace Microsoft::WRL;

class D3DRenderer
{
private:
	struct Vertex
	{
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT4 Color;
	};

	struct ConstantBuffer
	{
		DirectX::XMFLOAT4X4 WorldViewProj = d3dUtil::Identity4x4();
	};

	struct Mesh
	{
		template<typename VertexContainer, typename IndexContainer>
		Mesh(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const VertexContainer& vertices, const IndexContainer& indices)
		{
			const UINT vertexBufferSize = static_cast<UINT>(vertices.size() * sizeof(vertices[0]));
			const UINT indexBufferSize = static_cast<UINT>(indices.size() * sizeof(indices[0]));

			_vertexGPU = d3dUtil::CreateBuffer(device, cmdList, _vertexUpload, vertices.data(), vertexBufferSize);
			_indexGPU = d3dUtil::CreateBuffer(device, cmdList, _indexUpload, indices.data(), indexBufferSize);

			_vertexBufferView.BufferLocation = _vertexGPU->GetGPUVirtualAddress();
			_vertexBufferView.SizeInBytes = vertexBufferSize;
			_vertexBufferView.StrideInBytes = sizeof(vertices[0]);

			_indexBufferView.BufferLocation = _indexGPU->GetGPUVirtualAddress();
			_indexBufferView.SizeInBytes = indexBufferSize;
			_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
		}

		D3D12_VERTEX_BUFFER_VIEW _vertexBufferView{};
		D3D12_INDEX_BUFFER_VIEW _indexBufferView{};


	private:
		ComPtr<ID3D12Resource> _vertexUpload;
		ComPtr<ID3D12Resource> _indexUpload;

		ComPtr<ID3D12Resource> _vertexGPU;
		ComPtr<ID3D12Resource> _indexGPU;
	};

public:
	D3DRenderer(HWND wh, int w, int h);
	~D3DRenderer();
	
	D3DRenderer(const D3DRenderer&) = delete;
	D3DRenderer& operator=(const D3DRenderer&) = delete;

	void d3dInit();
	void RenderFrame(float dt);
	void Update(float dt);
	
	float AspectRatio() const noexcept;

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
	void OnResize();

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

	std::vector<Mesh> _objects;
	
	std::unique_ptr<d3dUtil::UploadBuffer<ConstantBuffer>> _constantBuffer;
	ComPtr<ID3D12DescriptorHeap> _cbvHeap;

	ComPtr<ID3D12PipelineState> _pso;
	ComPtr<ID3D12RootSignature> _rootSignature;
};