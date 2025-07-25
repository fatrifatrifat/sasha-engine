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

	struct SubmeshGeometry
	{
		UINT IndexCount = 0;
		UINT StartIndexLocation = 0;
		INT BaseVertexLocation = 0;

		// Bounding box of the geometry defined by this submesh. 
		// This is used in later chapters of the book.
		DirectX::BoundingBox Bounds;
	};

	struct MeshGeometry
	{
		// Give it a name so we can look it up by name.
		std::string Name;

		// System memory copies.  Use Blobs because the vertex/index format can be generic.
		// It is up to the client to cast appropriately.  
		Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
		Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

		Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

		Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

		// Data about the buffers.
		UINT VertexByteStride = 0;
		UINT VertexBufferByteSize = 0;
		DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
		UINT IndexBufferByteSize = 0;

		// A MeshGeometry may store multiple geometries in one vertex/index buffer.
		// Use this container to define the Submesh geometries so we can draw
		// the Submeshes individually.
		std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

		D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const
		{
			D3D12_VERTEX_BUFFER_VIEW vbv;
			vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
			vbv.StrideInBytes = VertexByteStride;
			vbv.SizeInBytes = VertexBufferByteSize;

			return vbv;
		}

		D3D12_INDEX_BUFFER_VIEW IndexBufferView()const
		{
			D3D12_INDEX_BUFFER_VIEW ibv;
			ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
			ibv.Format = IndexFormat;
			ibv.SizeInBytes = IndexBufferByteSize;

			return ibv;
		}

		// We can free this memory after we finish upload to the GPU.
		void DisposeUploaders()
		{
			VertexBufferUploader = nullptr;
			IndexBufferUploader = nullptr;
		}
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

	std::unique_ptr<MeshGeometry> _object;
	
	std::unique_ptr<d3dUtil::UploadBuffer<ConstantBuffer>> _constantBuffer;
	ComPtr<ID3D12DescriptorHeap> _cbvHeap;

	ComPtr<ID3D12PipelineState> _pso;
	ComPtr<ID3D12RootSignature> _rootSignature;
};