#pragma warning(disable:4221) // nonstandard extension used: array cannot be initialized using address of automatic variable
#pragma warning(disable:4204) // nonstandard extension used: non-constant aggregate initializer
#pragma warning(disable:4201) // nonstandard extension used: nameless struct/union

#define NOMINMAX
#include <windows.h>

#define COBJMACROS
#include <d3d12.h>
#include <dxgi1_6.h>

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

enum { NUM_RENDERTARGETS = 2 };

struct renderer_d3d12 {
	ID3D12Debug* debug;
	IDXGIFactory4* factory;
	IDXGIAdapter1* adapter;
	ID3D12Device* device;
	ID3D12CommandQueue* queue;
	IDXGISwapChain1* swapchain;
	ID3D12DescriptorHeap* rtvDescriptorHeap;
	ID3D12Resource* targets[NUM_RENDERTARGETS];

	// TODO free these
	ID3D12CommandAllocator* commandAllocator;
	ID3D12RootSignature* rootSignature;
};

bool renderer_init(struct renderer_d3d12* renderer, HWND window, uint32_t windowWidth, uint32_t windowHeight) {
	// debug reporting
	#if BUILD_DEBUG
	{
		if (SUCCEEDED(D3D12GetDebugInterface(&IID_ID3D12Debug, &renderer->debug))) {
			ID3D12Debug_EnableDebugLayer(renderer->debug);
		} else {
			printf("Failed to get debug interface\n");
			return false;
		}
	}
	#endif

	////////////////////////////////////////////////////////////////////////////////	
	// create pipeline objects
	{
		HRESULT  hr = CreateDXGIFactory1(&IID_IDXGIFactory4, &renderer->factory);
		if (!SUCCEEDED(hr)) {
			printf("Failed to create factory: %u\n", hr);
			return false;
		}
	}

	for (UINT i = 0; DXGI_ERROR_NOT_FOUND != IDXGIFactory1_EnumAdapters1(renderer->factory, i, &renderer->adapter); ++i) {
		DXGI_ADAPTER_DESC1 desc;
		IDXGIAdapter1_GetDesc1(renderer->adapter, &desc);
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			continue;
		}
		HRESULT hr = D3D12CreateDevice((IUnknown*)renderer->adapter, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, NULL);
		if (SUCCEEDED(hr)) {
			break;
		} else {
			printf("Failed to create device: %u\n", hr);
		}
	}

	if (renderer->adapter == NULL) {
		printf("No hardware adapter found. This system does not support D3D12.\n");
		return false;
	}

	{
		HRESULT hr = D3D12CreateDevice((IUnknown*)renderer->adapter, 
										D3D_FEATURE_LEVEL_11_0, 
										&IID_ID3D12Device, 
										&renderer->device);
		if (!SUCCEEDED(hr)) {
			printf("Failed to create device: %u\n", hr);
			return false;
		}
	}

	{
		D3D12_COMMAND_QUEUE_DESC desc = {.Type = D3D12_COMMAND_LIST_TYPE_DIRECT};

		HRESULT hr = ID3D12Device_CreateCommandQueue(renderer->device, &desc, &IID_ID3D12CommandQueue, &renderer->queue);
		if (!SUCCEEDED(hr)) {
			printf("Failed to create command queue: %u", hr);
			return false;
		}
	}

	IDXGISwapChain1* swapchain = NULL;
	{
		DXGI_SWAP_CHAIN_DESC1 desc = {
			.Width = windowWidth,
			.Height = windowHeight,
			.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
			.SampleDesc = {
				.Count = 1,
				.Quality = 0,
			},
			.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
			.BufferCount = NUM_RENDERTARGETS,
			.Scaling = DXGI_SCALING_ASPECT_RATIO_STRETCH,
			.SwapEffect = DXGI_SWAP_EFFECT_DISCARD,
			.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
		};

		HRESULT hr = IDXGIFactory4_CreateSwapChainForHwnd(
			renderer->factory, (IUnknown*)renderer->device, window, &desc, NULL, NULL, &swapchain);
		if (!SUCCEEDED(hr)) {
			printf("Failed to create swap chain: %u\n", hr);
			return false;
		}
	}
	
	// disable fullscreen transitions
	IDXGIFactory4_MakeWindowAssociation(renderer->factory, window, DXGI_MWA_NO_ALT_ENTER);

	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			.NumDescriptors = 0,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		};

		HRESULT hr = ID3D12Device_CreateDescriptorHeap(
			renderer->device, &desc, &IID_ID3D12DescriptorHeap, &renderer->rtvDescriptorHeap);
		if (!SUCCEEDED(hr)) {
			printf("Failed to create descriptor heap: %u\n", hr);
			return false;
		}
	}

	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptor = 
			ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(renderer->rtvDescriptorHeap);
		UINT rtvDescriptorSize = ID3D12Device_GetDescriptorHandleIncrementSize(renderer->device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		for (size_t i = 0; i < NUM_RENDERTARGETS; ++i) {
			HRESULT hr = IDXGISwapChain1_GetBuffer(renderer->swapchain, i, &IID_ID3D12Resource, &renderer->targets[i]);
			if (!SUCCEEDED(hr)) {
				return false;
			}
			ID3D12Device_CreateRenderTargetView(renderer->device, renderer->targets[i], NULL, rtvDescriptor);
			rtvDescriptor.ptr += rtvDescriptorSize;
		}
	}

	{
		HRESULT hr = 
			ID3D12Device_CreateCommandAllocator(renderer->device, 
												D3D12_COMMAND_LIST_TYPE_DIRECT, 
												&IID_ID3D12CommandAllocator,
												&renderer->commandAllocator);
		if (!SUCCEEDED(hr)) {
			printf("Failed to create command allocator: %u\n", hr);
			return false;
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	// create pipline assets
	
	ID3D12RootSignature* rootSignature = NULL;
	{
		ID3DBlob* signatureBlob = NULL;
		D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = {
			.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
			.Desc_1_1 = {
				.NumParameters = 0,
				.NumStaticSamplers = 0,
				.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
			},
		};
		HRESULT hr = D3D12SerializeVersionedRootSignature(&desc, &signatureBlob, NULL);
		if (!SUCCEEDED(hr)) {
			printf("Failed to serialize root signature: %u\n", hr);
			return false;
		}
		void* ptr = ID3D10Blob_GetBufferPointer(signatureBlob);
		size_t size = ID3D10Blob_GetBufferSize(signatureBlob);
		hr = ID3D12Device_CreateRootSignature(renderer->device, 0, ptr, size, &IID_ID3D12RootSignature, &rootSignature);
		if (!SUCCEEDED(hr)) {
			printf("Failed to create root signature: %u\n", hr);
			return false;
		}
	}


//
//    // Create the pipeline state, which includes compiling and loading shaders.
//    {
//        ComPtr<ID3DBlob> vertexShader;
//        ComPtr<ID3DBlob> pixelShader;
//
//#if defined(_DEBUG)
//        // Enable better shader debugging with the graphics debugging tools.
//        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
//#else
//        UINT compileFlags = 0;
//#endif
//
//        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
//        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));
//
//        // Define the vertex input layout.
//        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
//        {
//            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
//            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
//        };
//
//        // Describe and create the graphics pipeline state object (PSO).
//        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
//        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
//        psoDesc.pRootSignature = m_rootSignature.Get();
//        psoDesc.VS = { reinterpret_cast<UINT8*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize() };
//        psoDesc.PS = { reinterpret_cast<UINT8*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };
//        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
//        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
//        psoDesc.DepthStencilState.DepthEnable = FALSE;
//        psoDesc.DepthStencilState.StencilEnable = FALSE;
//        psoDesc.SampleMask = UINT_MAX;
//        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
//        psoDesc.NumRenderTargets = 1;
//        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
//        psoDesc.SampleDesc.Count = 1;
//        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
//    }
//
//    // Create the command list.
//    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));
//
//    // Command lists are created in the recording state, but there is nothing
//    // to record yet. The main loop expects it to be closed, so close it now.
//    ThrowIfFailed(m_commandList->Close());
//
//    // Create the vertex buffer.
//    {
//        // Define the geometry for a triangle.
//        Vertex triangleVertices[] =
//        {
//            { { 0.0f, 0.25f * m_aspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
//            { { 0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
//            { { -0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
//        };
//
//        const UINT vertexBufferSize = sizeof(triangleVertices);
//
//        // Note: using upload heaps to transfer static data like vert buffers is not 
//        // recommended. Every time the GPU needs it, the upload heap will be marshalled 
//        // over. Please read up on Default Heap usage. An upload heap is used here for 
//        // code simplicity and because there are very few verts to actually transfer.
//        ThrowIfFailed(m_device->CreateCommittedResource(
//            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
//            D3D12_HEAP_FLAG_NONE,
//            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
//            D3D12_RESOURCE_STATE_GENERIC_READ,
//            nullptr,
//            IID_PPV_ARGS(&m_vertexBuffer)));
//
//        // Copy the triangle data to the vertex buffer.
//        UINT8* pVertexDataBegin;
//        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
//        ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
//        memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
//        m_vertexBuffer->Unmap(0, nullptr);
//
//        // Initialize the vertex buffer view.
//        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
//        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
//        m_vertexBufferView.SizeInBytes = vertexBufferSize;
//    }
//
//    // Create synchronization objects and wait until assets have been uploaded to the GPU.
//    {
//        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
//        m_fenceValue = 1;
//
//        // Create an event handle to use for frame synchronization.
//        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
//        if (m_fenceEvent == nullptr)
//        {
//            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
//        }
//
//        // Wait for the command list to execute; we are reusing the same command 
//        // list in our main loop but for now, we just want to wait for setup to 
//        // complete before continuing.
//        WaitForPreviousFrame();
//    }

	return true;
}

void renderer_free(struct renderer_d3d12* renderer) {
	ID3D12Debug_Release(renderer->debug);
	IDXGIFactory4_Release(renderer->factory);
	IDXGIAdapter1_Release(renderer->adapter);
	ID3D12Device_Release(renderer->device);
	ID3D12CommandQueue_Release(renderer->queue);
	IDXGISwapChain1_Release(renderer->swapchain);
	ID3D12DescriptorHeap_Release(renderer->rtvDescriptorHeap);
	ID3D12CommandAllocator_Release(renderer->commandAllocator);

	for (size_t i = 0; i < NUM_RENDERTARGETS; ++i) {
		ID3D12Resource_Release(renderer->targets[i]);
	}
}

void renderer_update(struct renderer_d3d12* ) {
	// populate command list
	// execute command list
	// present
	// wait for frame
}

LRESULT CALLBACK WindowProc(HWND hWindow, UINT msg, WPARAM wparam, LPARAM lparam)
{
    const POINT kMinSize = {1, 1};
    switch (msg)
    {
    case WM_CLOSE:
        PostQuitMessage(0);
        break;
    case WM_PAINT:
        // draw graphics
        break;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO*)lparam)->ptMinTrackSize = kMinSize;
        break;
    case WM_SIZE:
        // TODO handle resizing
        break;
    }
    return DefWindowProc(hWindow, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR cmdline, int nCmdShow)
{
    (void)hPrevInstance;
    (void)cmdline;
    (void)nCmdShow;

    WNDCLASSEX win_class = {0};
    win_class.cbSize = sizeof(WNDCLASSEX);
    win_class.style = CS_HREDRAW | CS_VREDRAW;
    win_class.lpfnWndProc = WindowProc;
    win_class.hInstance = hInstance;
    win_class.lpszClassName = "DemoWindow";
    if (!RegisterClassEx(&win_class))
    {
        printf("Failed to create window class.");
        return 1;
    }

	const uint32_t WINDOW_WIDTH = 1280;
	const uint32_t WINDOW_HEIGHT = 720;

    HWND window = CreateWindowEx(0,
                                 "DemoWindow",
                                 "pbr_demo",
                                 WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_SYSMENU,
                                 100, 100,
                                 WINDOW_WIDTH, WINDOW_HEIGHT,
                                 NULL,
                                 NULL,
                                 hInstance,
                                 NULL);
    if (!window)
    {
        printf("Failed to create window.");
        return 1;
    }

	struct renderer_d3d12 renderer = {0};
	if (!renderer_init(&renderer, window, WINDOW_WIDTH, WINDOW_HEIGHT)) {
		return 1;
	}

	////////////////////////////////////////////////////////////////////////////////	
    {
        MSG msg;
        bool done = false;
        while (!done)
        {
            PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
            if (msg.message == WM_QUIT)
            {
                done = true;
            }
            else
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

			renderer_update(&renderer);

            RedrawWindow(window, NULL, NULL, RDW_INTERNALPAINT);
        }
    }

	renderer_free(&renderer);

    return 0;
}
