#pragma warning(disable:4221) // nonstandard extension used: array cannot be initialized using address of automatic variable
#pragma warning(disable:4204) // nonstandard extension used: non-constant aggregate initializer
#pragma warning(disable:4201) // nonstandard extension used: nameless struct/union

#define NOMINMAX
#include <windows.h>

#define COBJMACROS
	#pragma warning(push)
	#pragma warning(disable:4115) // named type definition in parentheses
	#include <d3d12.h>
	#include <d3dcompiler.h>
	#include <dxgi1_6.h>
	#pragma warning(pop)
#undef COBJMACROS

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "vec.h"

#define STATIC_ARRAY_LENGTH(array) (sizeof(array) / sizeof(*(array)))

////////////////////////////////////////////////////////////////////////////////

bool readFile(const char* path, char** o_buffer, size_t* o_size)
{
	HANDLE file = CreateFile(path,
		GENERIC_READ,
		FILE_SHARE_READ,       // share for reading
		NULL,                  // default security
		OPEN_EXISTING,         // existing file only
		FILE_ATTRIBUTE_NORMAL, // normal file
		NULL);                 // no attr. template	if (file == INVALID_HANDLE_VALUE)

	if (file == INVALID_HANDLE_VALUE) {
		printf("Failed to open file \"%s\" handle\n", path);
		return false;
	}

	LARGE_INTEGER size = { 0 };
	BOOL success = GetFileSizeEx(file, &size);
	if (!success) {
		printf("Failed to get file size\n");
		return false;
	}

	*o_size = (size_t)size.QuadPart;

	if (*o_size == 0) {
		printf("File \"%s\" size was 0\n", path);
		return false;
	}

	// We add an extra 1 to the end of the filesize to null-terminate it in case it's a 
	// text file to simplify resource loading code.
	*o_buffer = malloc(*o_size + 1);
	(*o_buffer)[*o_size] = 0;

	DWORD bytesRead = 0;
	success = ReadFile(file, *o_buffer, *o_size, &bytesRead, NULL);
	CloseHandle(file);

	return success;
}

////////////////////////////////////////////////////////////////////////////////

D3D12_CPU_DESCRIPTOR_HANDLE get_descriptor_handle_d3d12(ID3D12DescriptorHeap* heap) {
	// NOTE - The Microsoft declaration for this function is broken for the C interface
	typedef void(*GetCpuHandleHack)(ID3D12DescriptorHeap*, D3D12_CPU_DESCRIPTOR_HANDLE*);
	GetCpuHandleHack func = (GetCpuHandleHack)heap->lpVtbl->GetCPUDescriptorHandleForHeapStart;

	D3D12_CPU_DESCRIPTOR_HANDLE handle = { 0 };
	func(heap, &handle);
	return handle;
}

////////////////////////////////////////////////////////////////////////////////

struct windowsize {
	uint32_t width;
	uint32_t height;
};

enum { NUM_RENDERTARGETS = 2 };

struct renderer_d3d12 {
	ID3D12Debug* debug;
	IDXGIFactory4* factory;
	IDXGIAdapter1* adapter;
	ID3D12Device* device;
	ID3D12CommandQueue* queue;
	IDXGISwapChain4* swapchain;
	ID3D12DescriptorHeap* rtvDescriptorHeap;
	ID3D12Resource* targets[NUM_RENDERTARGETS]; // TODO move to resources_d3d12 stuct
	ID3D12CommandAllocator* commandAllocator;
	ID3D12RootSignature* rootSignature;
	ID3D12PipelineState* pipeline;
	ID3D12GraphicsCommandList* cmdlist;
};

struct resources_d3d12 {
	ID3D12Resource* vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	ID3D12Fence* fence;
	HANDLE fenceEvent;
	uint32_t fenceValue;
	UINT frameIndex;
};

struct vertex {
	struct vec3f pos;
	struct vec4f color;
};

void wait_for_frame(struct renderer_d3d12* renderer, struct resources_d3d12* resources);

bool renderer_init(struct renderer_d3d12* renderer, struct resources_d3d12* resources, HWND window, struct windowsize ws) {
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

	{
		DXGI_SWAP_CHAIN_DESC1 desc = {
			.Width = ws.width,
			.Height = ws.height,
			.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
			.SampleDesc = {
				.Count = 1,
				.Quality = 0,
			},
			.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
			.BufferCount = NUM_RENDERTARGETS,
			.Scaling = DXGI_SCALING_NONE,
			.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
			.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
		};

		HRESULT hr = IDXGIFactory4_CreateSwapChainForHwnd(
			renderer->factory, (IUnknown*)renderer->queue, window, &desc, NULL, NULL, (IDXGISwapChain1**)&renderer->swapchain);
		if (!SUCCEEDED(hr)) {
			printf("Failed to create swap chain: %u\n", hr);
			return false;
		}
	}
	
	// disable fullscreen transitions
	IDXGIFactory4_MakeWindowAssociation(renderer->factory, window, DXGI_MWA_NO_ALT_ENTER);

	resources->frameIndex = IDXGISwapChain3_GetCurrentBackBufferIndex(renderer->swapchain);

	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {
			.NumDescriptors = NUM_RENDERTARGETS,
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
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
		const UINT rtvDescriptorSize = 
			ID3D12Device_GetDescriptorHandleIncrementSize(renderer->device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle = get_descriptor_handle_d3d12(renderer->rtvDescriptorHeap);

		for (size_t i = 0; i < NUM_RENDERTARGETS; ++i) {
			HRESULT hr = IDXGISwapChain1_GetBuffer(renderer->swapchain, i, &IID_ID3D12Resource, &renderer->targets[i]);
			if (!SUCCEEDED(hr)) {
				return false;
			}
			ID3D12Device_CreateRenderTargetView(renderer->device, renderer->targets[i], NULL, rtvDescriptorHandle);
			rtvDescriptorHandle.ptr += rtvDescriptorSize;
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
	
	{
		D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = {
			.Version = D3D_ROOT_SIGNATURE_VERSION_1_0,
			.Desc_1_0 = {
				.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
			},
		};
		ID3DBlob* serializedDesc = NULL;
		HRESULT hr = D3D12SerializeVersionedRootSignature(&desc, &serializedDesc, NULL);
		if (!SUCCEEDED(hr)) {
			printf("Failed to serialize root signature: %u\n", hr);
			return false;
		}

		hr = ID3D12Device_CreateRootSignature(renderer->device, 0,
			ID3D10Blob_GetBufferPointer(serializedDesc),
			ID3D10Blob_GetBufferSize(serializedDesc),
			&IID_ID3D12RootSignature,
			&renderer->rootSignature);

		if (!SUCCEEDED(hr)) {
			printf("Failed to create root signature: %u\n", hr);
			return false;
		}

		ID3D10Blob_Release(serializedDesc);
	}


	{
		ID3DBlob* vs;
		ID3DBlob* ps;

		char* data = NULL;
		size_t filesize = 0;
		if (!readFile("shaders.hlsl", &data, &filesize)) {
			return false;
		}

		UINT compileFlags = 0;
		#if BUILD_DEBUG
			compileFlags |= D3DCOMPILE_DEBUG;
			compileFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
		#endif

		HRESULT hr = D3DCompile(data, filesize, NULL, NULL, NULL, "VSMain", "vs_4_0", compileFlags, 0, &vs, NULL);
		if (!SUCCEEDED(hr)) {
			printf("Failed to compile vertex shader: %u\n", hr);
			return false;
		}
		hr = D3DCompile(data, filesize, NULL, NULL, NULL, "PSMain", "ps_4_0", compileFlags, 0, &ps, NULL);
		if (!SUCCEEDED(hr)) {
			printf("Failed to compile vertex shader: %u\n", hr);
			return false;
		}

		D3D12_INPUT_ELEMENT_DESC vertexFormat[] = {
			{ 
				.SemanticName = "POSITION", 
				.Format = DXGI_FORMAT_R32G32B32_FLOAT, 
				.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 
			},
			{ 	
				.SemanticName = "COLOR", 
				.Format = DXGI_FORMAT_R32G32B32A32_FLOAT, 
				.AlignedByteOffset = sizeof(float) * 3, 
				.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 
			},
		};

		const D3D12_RENDER_TARGET_BLEND_DESC defaultBlendState = {
			.BlendEnable = FALSE,
			.LogicOpEnable = FALSE,

			.SrcBlend = D3D12_BLEND_ONE,
			.DestBlend = D3D12_BLEND_ZERO,
			.BlendOp = D3D12_BLEND_OP_ADD,

			.SrcBlendAlpha = D3D12_BLEND_ONE,
			.DestBlendAlpha = D3D12_BLEND_ZERO,
			.BlendOpAlpha = D3D12_BLEND_OP_ADD,

			.LogicOp = D3D12_LOGIC_OP_NOOP,
			.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {
			.pRootSignature = renderer->rootSignature,
			.VS = {
				.pShaderBytecode = ID3D10Blob_GetBufferPointer(vs),
				.BytecodeLength = ID3D10Blob_GetBufferSize(vs),
			},
			.PS = {
				.pShaderBytecode = ID3D10Blob_GetBufferPointer(ps),
				.BytecodeLength = ID3D10Blob_GetBufferSize(ps),
			},
			.StreamOutput = {0},
			.BlendState = {
				.AlphaToCoverageEnable = FALSE,
				.IndependentBlendEnable = FALSE, // TODO may want to set to TRUE if we end up having multiple render targets
				.RenderTarget = { defaultBlendState }, // TODO doesn't support multiple render targets
			},
			.SampleMask = 0xFFFFFFFF,
			.RasterizerState = {
				.FillMode = D3D12_FILL_MODE_SOLID, // D3D_FILL_MODE_WIREFRAME
				.CullMode = D3D12_CULL_MODE_BACK,
				.FrontCounterClockwise = FALSE,
				.DepthBias = 0,
				.DepthBiasClamp = 0,
				.SlopeScaledDepthBias = 0,
				.DepthClipEnable = TRUE,
				.MultisampleEnable = FALSE,
				.AntialiasedLineEnable = FALSE,
				.ForcedSampleCount = 0,
				.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
			},
			.DepthStencilState = {
				.DepthEnable = FALSE,
				.StencilEnable = FALSE,
			},
			.InputLayout = {
				.pInputElementDescs = vertexFormat,
				.NumElements = STATIC_ARRAY_LENGTH(vertexFormat),
			},
			//.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
			.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			.NumRenderTargets = 1,
			.RTVFormats = { DXGI_FORMAT_R8G8B8A8_UNORM }, // TODO only supports 1 render target
			.DSVFormat = DXGI_FORMAT_UNKNOWN, // TODO ?
			.SampleDesc = { // TODO figure out if this info should be shared between the swapchain and this?
				.Count = 1,
				.Quality = 0,
			},
		};
		
		hr = ID3D12Device_CreateGraphicsPipelineState(
			renderer->device, &pipelineStateDesc, &IID_ID3D12PipelineState, &renderer->pipeline);
		if (!SUCCEEDED(hr)) {
			printf("Failed to create pipeline state: %u\n", hr);
			return false;
		}

		ID3D10Blob_Release(vs);
		ID3D10Blob_Release(ps);
	}

	{
		HRESULT hr = ID3D12Device_CreateCommandList(renderer->device, 
													0, 
													D3D12_COMMAND_LIST_TYPE_DIRECT, 
													renderer->commandAllocator, 
													renderer->pipeline,
													&IID_ID3D12CommandList,
													&renderer->cmdlist);
		if (!SUCCEEDED(hr)) {
			printf("Failed to create command list: %u\n", hr);
			return false;
		}

		// command lists begin in the recording state, but our Update() opens it
		hr = ID3D12GraphicsCommandList_Close(renderer->cmdlist);
		if (!SUCCEEDED(hr)) {
			printf("Failed to close command list: %u\n", hr);
			return false;
		}
	}

	// TODO research more efficient way of uploading vertex buffers in the future
	{
		float aspect = (float)ws.width / (float)ws.height;
		struct vertex vertices[] = {
			{ .pos = { 0, .25f * aspect, 0.0f, }, .color = {1,0,0,0} },
			{ .pos = { .25f, -.25f * aspect, 0.0f, }, .color = {1,0,0,0} },
			{ .pos = { -.25f, -.25f * aspect, 0.0f, }, .color = {1,0,0,0} },
		};

		const D3D12_HEAP_PROPERTIES heapProps = {
			.Type = D3D12_HEAP_TYPE_UPLOAD,
		};

		const D3D12_RESOURCE_DESC resourceDesc = {
			.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
			.Alignment = 0,
			.Width = sizeof(vertices),
			.Height = 1,
			.DepthOrArraySize = 1,
			.MipLevels = 1,
			.Format = DXGI_FORMAT_UNKNOWN,
			.SampleDesc = { .Count = 1, .Quality = 0 },
			.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
			.Flags = D3D12_RESOURCE_FLAG_NONE,
		};

		HRESULT hr = ID3D12Device_CreateCommittedResource(
			renderer->device, 
			&heapProps, 
			D3D12_HEAP_FLAG_NONE, 
			&resourceDesc, 
			D3D12_RESOURCE_STATE_GENERIC_READ, 
			NULL,
			&IID_ID3D12Resource, 
			&resources->vertexBuffer);
		if (!SUCCEEDED(hr)) {
			printf("Failed to create vertex buffer resource: %d\n", hr);
			return false;
		}

		void* gpuData = NULL;
		D3D12_RANGE readRange = { 0 }; // cpu isn't going to read this data, only write
		hr = ID3D12Resource_Map(resources->vertexBuffer, 0, &readRange, &gpuData);
		if (!SUCCEEDED(hr)) {
			printf("Failed to create vertex buffer resource: %d\n", hr);
			return false;
		}
		memcpy(gpuData, vertices, sizeof(vertices));
		ID3D12Resource_Unmap(resources->vertexBuffer, 0, NULL);

		D3D12_VERTEX_BUFFER_VIEW vbView = {
			.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(resources->vertexBuffer),
			.StrideInBytes = sizeof(struct vertex),
			.SizeInBytes = sizeof(vertices),
		};
		resources->vertexBufferView = vbView;
	}

	{
		HRESULT hr = ID3D12Device_CreateFence(
			renderer->device, resources->fenceValue, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, &resources->fence);
		if (!SUCCEEDED(hr)) {
			printf("Failed to create fence: %d\n", hr);
			return false;
		}
		++resources->fenceValue;

		BOOL manualReset = FALSE;
		BOOL initialState = FALSE;
		resources->fenceEvent = CreateEvent(0, manualReset, initialState, NULL);
		if (resources->fenceEvent == NULL) {
			hr = HRESULT_FROM_WIN32(GetLastError());
			printf("Failed to create fence event: %d\n", hr);
			return false;
		}
	}

	// wait for gpu to finish work before continuing
	wait_for_frame(renderer, resources);

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
	ID3D12RootSignature_Release(renderer->rootSignature);
	ID3D12PipelineState_Release(renderer->pipeline);
	ID3D12GraphicsCommandList_Release(renderer->cmdlist);

	for (size_t i = 0; i < NUM_RENDERTARGETS; ++i) {
		ID3D12Resource_Release(renderer->targets[i]);
	}
}

void resources_free(struct resources_d3d12* resources) {
	ID3D12Resource_Release(resources->vertexBuffer);
	ID3D12Fence_Release(resources->fence);
	CloseHandle(resources->fenceEvent);
}

void wait_for_frame(struct renderer_d3d12* renderer, struct resources_d3d12* resources) {
	// TODO make this more efficient
	uint32_t value = resources->fenceValue;

	HRESULT hr = ID3D12CommandQueue_Signal(renderer->queue, resources->fence, value);
	if (!SUCCEEDED(hr)) {
		printf("Failed to signal fence: %d\n", hr);
		return;
	}

	++resources->fenceValue;
	
	if (ID3D12Fence_GetCompletedValue(resources->fence) < value) {
		hr = ID3D12Fence_SetEventOnCompletion(resources->fence, value, resources->fenceEvent);
		if (!SUCCEEDED(hr)) {
			printf("Failed to set event on completion flag: %d\n", hr);
		}
		WaitForSingleObject(resources->fenceEvent, INFINITE);
	}

	resources->frameIndex = IDXGISwapChain3_GetCurrentBackBufferIndex(renderer->swapchain);
}

void renderer_update(struct renderer_d3d12* renderer, struct resources_d3d12* resources, struct windowsize ws) {
	// populate command list

	// NOTE - use fences to make sure associated command lists are finished executing
	// TODO inline wait for frame in here??
	HRESULT hr = ID3D12CommandAllocator_Reset(renderer->commandAllocator);
	if (!SUCCEEDED(hr)) {
		printf("Failed to reset command allocator: %d\n", hr);
		return;
	}

	hr = ID3D12GraphicsCommandList_Reset(renderer->cmdlist, renderer->commandAllocator, renderer->pipeline);
	if (!SUCCEEDED(hr)) {
		printf("Failed to reset command list: %d\n", hr);
		return;
	}

	const D3D12_VIEWPORT viewport = {
		.TopLeftX = 0.0f,
		.TopLeftY = 0.0f,
		.Width = (float)ws.width,
		.Height = (float)ws.height,
		.MinDepth = (float)0.0f,
		.MaxDepth = (float)0.0f,
	};

	const D3D12_RECT scissorRect = {
		.left = 0, .right = ws.width,
		.top = 0, .bottom = ws.height,
	};

	// This state is reset everytime the cmd list is reset, so we need to rebind it
	ID3D12GraphicsCommandList_SetGraphicsRootSignature(renderer->cmdlist, renderer->rootSignature);
	ID3D12GraphicsCommandList_RSSetViewports(renderer->cmdlist, 1, &viewport);
	ID3D12GraphicsCommandList_RSSetScissorRects(renderer->cmdlist, 1, &scissorRect);

	const D3D12_RESOURCE_BARRIER toRenderTargetBarrier = {
		.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
		.Transition = {
			.pResource = renderer->targets[resources->frameIndex],
			.StateBefore = D3D12_RESOURCE_STATE_PRESENT,
			.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
			.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
		},
	};
	ID3D12GraphicsCommandList_ResourceBarrier(renderer->cmdlist, 1, &toRenderTargetBarrier);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = get_descriptor_handle_d3d12(renderer->rtvDescriptorHeap);
	ID3D12GraphicsCommandList_OMSetRenderTargets(renderer->cmdlist, 1, &rtvHandle, FALSE, NULL);

	// clear backbuffer
	const float clearcolor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	ID3D12GraphicsCommandList_ClearRenderTargetView(renderer->cmdlist, rtvHandle, clearcolor, 0, NULL);

	// draw calls!
	//ID3D12GraphicsCommandList_IASetPrimitiveTopology(renderer->cmdlist, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//ID3D12GraphicsCommandList_IASetVertexBuffers(renderer->cmdlist, 0, 1, resources->vertexBufferView);
	//ID3D12GraphicsCommandList_DrawInstanced(renderer->cmdlist, 3, 1, 0, 0);
	
	D3D12_RESOURCE_BARRIER toPresentBarrier = toRenderTargetBarrier;
	toPresentBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	toPresentBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

	ID3D12GraphicsCommandList_ResourceBarrier(renderer->cmdlist, 1, &toPresentBarrier);
	ID3D12GraphicsCommandList_Close(renderer->cmdlist);

	// execute
    ID3D12GraphicsCommandList* cmdlists[] = { renderer->cmdlist };
    ID3D12CommandQueue_ExecuteCommandLists(renderer->queue, STATIC_ARRAY_LENGTH(cmdlists), (ID3D12CommandList**)cmdlists);

	// present
	{
		UINT flags = 0;
		DXGI_PRESENT_PARAMETERS params = {0};
		hr = IDXGISwapChain1_Present1(renderer->swapchain, 1, flags, &params);
		if (!SUCCEEDED(hr)) {
			printf("Failed to present backbuffer: %u\n", hr);
		}
	}
	// wait for frame
}

LRESULT CALLBACK WindowProc(HWND hWindow, UINT msg, WPARAM wparam, LPARAM lparam) {
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR cmdline, int nCmdShow) {
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

	struct windowsize ws = { .width = WINDOW_WIDTH, .height = WINDOW_HEIGHT };
	struct renderer_d3d12 renderer = {0};
	struct resources_d3d12 resources = {0};
	if (!renderer_init(&renderer, &resources, window, ws)) {
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

			renderer_update(&renderer, &resources, ws);
			wait_for_frame(&renderer, &resources);

            RedrawWindow(window, NULL, NULL, RDW_INTERNALPAINT);
        }
    }

	wait_for_frame(&renderer, &resources);

	renderer_free(&renderer);
	resources_free(&resources);

    return 0;
}

