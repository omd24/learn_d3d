#include <stdio.h>

// DirectX 12 specific headers.
#include <d3d12.h>

#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

// -- uncomment below to use CHelper structs
//#include "d3dx12.h"

#if !defined(NDEBUG) && !defined(_DEBUG)
#error "Define at least one."
#elif defined(NDEBUG) && defined(_DEBUG)
#error "Define at most one."
#endif

#if defined(_WIN64)
#if defined(_DEBUG)
#pragma comment (lib, "lib/64/SDL2-staticd")
#else
#pragma comment (lib, "lib/64/SDL2-static")
#endif
#else
#if defined(_DEBUG)
#pragma comment (lib, "lib/32/SDL2-staticd")
#else
#pragma comment (lib, "lib/32/SDL2-static")
#endif
#endif

#pragma comment (lib, "Imm32")
#pragma comment (lib, "Setupapi")
#pragma comment (lib, "Version")
#pragma comment (lib, "Winmm")

#pragma comment (lib, "d3d12")
#pragma comment (lib, "dxgi")
#pragma comment (lib, "dxguid")
#pragma comment (lib, "uuid")
#pragma comment (lib, "gdi32")
#pragma comment (lib, "d3dcompiler")

#define _CRT_SECURE_NO_WARNINGS 1

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <dxgidebug.h>
//#include <directxmath.h>

#define SDL_MAIN_HANDLED
#include "SDL2/SDL.h"
#include "SDL2/SDL_syswm.h"

#include <cstdio>
#include <algorithm>

//// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
//#include <wrl.h>
//using namespace Microsoft::WRL;

/*
d3d12.lib
dxgi.lib
dxguid.lib
uuid.lib
kernel32.lib
user32.lib
gdi32.lib
winspool.lib
comdlg32.lib
advapi32.lib
shell32.lib
ole32.lib
oleaut32.lib
odbc32.lib
odbccp32.lib
runtimeobject.lib
*/

#define SUCCEEDED(hr)   (((HRESULT)(hr)) >= 0)
//#define SUCCEEDED_OPERATION(hr)   (((HRESULT)(hr)) == S_OK)
#define FAILED(hr)      (((HRESULT)(hr)) < 0)
//#define FAILED_OPERATION(hr)      (((HRESULT)(hr)) != S_OK)
#define CHECK_AND_FAIL(hr)                          \
    if (FAILED(hr)) {                               \
        ::printf("[ERROR] " #hr "() failed at line %d. \n", __LINE__);   \
        ::abort();                                  \
    }                                               \
    /**/

#if defined(_DEBUG)
#define ENABLE_DEBUG_LAYER 1
#else
#define ENABLE_DEBUG_LAYER 0
#endif

#if defined(_DEBUG)
UINT compiler_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
UINT compiler_flags = 0;
#endif

#define ARRAY_COUNT(arr)   sizeof(arr)/sizeof(arr[0])

#define FRAME_COUNT 2               // Use double-buffering
struct D3DRenderContext {
    
    // Display data
    UINT width;
    UINT height;
    float aspect_ratio;

    // Pipeline stuff
    D3D12_VIEWPORT                  viewport;
    D3D12_RECT                      scissor_rect;
    IDXGISwapChain3 *               swapchain3;
    IDXGISwapChain *                swapchain;
    ID3D12Device *                  device;
    ID3D12Resource *                render_targets [FRAME_COUNT];
    ID3D12CommandAllocator *        cmd_allocator;
    ID3D12CommandQueue *            cmd_queue;
    ID3D12RootSignature *           root_signature;
    ID3D12DescriptorHeap *          rtv_heap;
    ID3D12PipelineState *           pso;
    ID3D12GraphicsCommandList *     direct_cmd_list;
    UINT                            rtv_descriptor_size;

    // App resources
    ID3D12Resource *                vertex_buffer;
    D3D12_VERTEX_BUFFER_VIEW        vb_view;

    // Synchronization stuff
    UINT                            frame_index;
    HANDLE                          fence_event;
    ID3D12Fence *                   fence;
    UINT64                          fence_value;

};
struct Vertex {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 color;
};
static HRESULT
wait_for_previous_frame (D3DRenderContext * render_ctx) {
    // NOTE(omid):  We wait for the command list to execute; we are reusing the same command 
    //              list in our main loop but for now, we just want to wait for setup to 
    //              complete before continuing.

    // -- Caveat emptor:
    // NOTE(omid):  WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    //              This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    //              sample illustrates how to use fences for efficient resource usage and to
    //              maximize GPU utilization.

    HRESULT ret = E_FAIL;

    // -- 1. signal and increment the fence value:
    UINT64 curr_fence_val = render_ctx->fence_value;
    ret = render_ctx->cmd_queue->Signal(render_ctx->fence, curr_fence_val);
    CHECK_AND_FAIL(ret);
    ++(render_ctx->fence_value);

    // -- 2. wait until the previous frame is finished
    if (render_ctx->fence->GetCompletedValue() < curr_fence_val) {
        ret = render_ctx->fence->SetEventOnCompletion(curr_fence_val, render_ctx->fence_event);
        CHECK_AND_FAIL(ret);
        WaitForSingleObject(render_ctx->fence_event, INFINITE /*return only when the object is signaled*/);
    }

    // -- 3. update frame index
    render_ctx->frame_index = render_ctx->swapchain3->GetCurrentBackBufferIndex();

    return ret;
}
static HRESULT
render_triangle (D3DRenderContext * render_ctx) {
    
    HRESULT ret = E_FAIL;

    // Populate command list
    
    // -- reset cmd_allocator and cmd_list
    CHECK_AND_FAIL(render_ctx->cmd_allocator->Reset());
    ret = render_ctx->direct_cmd_list->Reset(render_ctx->cmd_allocator, render_ctx->pso);
    CHECK_AND_FAIL(ret);

    // -- set root_signature, viewport and scissor
    render_ctx->direct_cmd_list->SetGraphicsRootSignature(render_ctx->root_signature);

    render_ctx->direct_cmd_list->RSSetViewports(1, &render_ctx->viewport);
    render_ctx->direct_cmd_list->RSSetScissorRects(1, &render_ctx->scissor_rect);

    // -- indicate that the backbuffer will be used as the render target
    D3D12_RESOURCE_BARRIER barrier1 = {};
    barrier1.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier1.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier1.Transition.pResource = render_ctx->render_targets[render_ctx->frame_index];
    barrier1.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier1.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier1.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

    render_ctx->direct_cmd_list->ResourceBarrier(1, &barrier1);
    
    // -- get CPU descriptor handle that represents the start of the heap
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle = render_ctx->rtv_heap->GetCPUDescriptorHandleForHeapStart();
    // -- apply initial offset
    cpu_descriptor_handle.ptr = SIZE_T(INT64(cpu_descriptor_handle.ptr) + INT64(render_ctx->frame_index) * INT64(render_ctx->rtv_descriptor_size));
    render_ctx->direct_cmd_list->OMSetRenderTargets(1, &cpu_descriptor_handle, FALSE, nullptr);
    
    // -- record commands
    
    // 1 - set all the elements in a render target to one value.
    float clear_colors [] = {0.0f, 0.2f, 0.4f, 1.0f};
    render_ctx->direct_cmd_list->ClearRenderTargetView(cpu_descriptor_handle, clear_colors, 0, nullptr);

    // 2 - set primitive type and data order that describes input data for the input assembler stage.
    render_ctx->direct_cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 3 - set the CPU descriptor handle for the vertex buffers (one vb for now)
    render_ctx->direct_cmd_list->IASetVertexBuffers(0 , 1, &render_ctx->vb_view);

    // 4 - draws non-indexed, instanced primitives. A draw API submits work to the rendering pipeline.
    render_ctx->direct_cmd_list->DrawInstanced(
        3,  /* number of vertices to draw.                                                          */
        1,  /* number of instances to draw.                                                         */
        0,  /* index of the first vertex                                                            */
        0   /* a value added to each index before reading per-instance data from a vertex buffer    */
    );

    // -- indicate that the backbuffer will now be used to present
    D3D12_RESOURCE_BARRIER barrier2 = {};
    barrier2.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier2.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier2.Transition.pResource = render_ctx->render_targets[render_ctx->frame_index];
    barrier2.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier2.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier2.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

    render_ctx->direct_cmd_list->ResourceBarrier(1 , &barrier2);

    // -- finish populating command list
    render_ctx->direct_cmd_list->Close();

    ID3D12CommandList * cmd_lists [] = {render_ctx->direct_cmd_list};
    render_ctx->cmd_queue->ExecuteCommandLists(ARRAY_COUNT(cmd_lists), cmd_lists);

    render_ctx->swapchain->Present(1 /*sync interval*/, 0 /*present flag*/);

    return ret;
}
static void
create_triangle_vertices (float aspect_ratio, Vertex out_vertices []) {
    Vertex v_red = {};
    v_red.position.x = 0.0f;
    v_red.position.y = 0.25f * aspect_ratio;
    v_red.position.z = 0.0f;
    v_red.color.x = 1.0f;
    v_red.color.y = 0.0f;
    v_red.color.z = 0.0f;
    v_red.color.w = 1.0f;

    Vertex v_green = {};
    v_green.position.x = 0.25f;
    v_green.position.y = -0.25f * aspect_ratio;
    v_green.position.z = 0.0f;
    v_green.color.x = 0.0f;
    v_green.color.y = 1.0f;
    v_green.color.z = 0.0f;
    v_green.color.w = 1.0f;

    Vertex v_blue = {};
    v_blue.position.x = -0.25f;
    v_blue.position.y = -0.25f * aspect_ratio;
    v_blue.position.z = 0.0f;
    v_blue.color.x = 0.0f;
    v_blue.color.y = 0.0f;
    v_blue.color.z = 1.0f;
    v_blue.color.w = 1.0f;

    out_vertices[0] = v_red;
    out_vertices[1] = v_green;
    out_vertices[2] = v_blue;
}

int
main () {
    // Enable Debug Layer
    UINT dxgiFactoryFlags = 0;
#if ENABLE_DEBUG_LAYER > 0
    ID3D12Debug * debug_interface_dx = nullptr;
    //ID3D12Debug1 * debug_controller = nullptr;
    if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface_dx)))) {
        debug_interface_dx->EnableDebugLayer();
        //debug_interface_dx->QueryInterface(IID_PPV_ARGS(&debug_controller));
        //debug_controller->SetEnableGPUBasedValidation(true); // -- not needed for now
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    // ========================================================================================================
    // -- Initialization

    D3DRenderContext render_ctx = {.width = 1280, .height = 720};
    render_ctx.aspect_ratio = (float)render_ctx.width / (float)render_ctx.height;
    render_ctx.viewport.TopLeftX = 0;
    render_ctx.viewport.TopLeftY = 0;
    render_ctx.viewport.Width = (float)render_ctx.width;
    render_ctx.viewport.Height = (float)render_ctx.height;
    render_ctx.scissor_rect.left = 0;
    render_ctx.scissor_rect.top = 0;
    render_ctx.scissor_rect.right = render_ctx.width;
    render_ctx.scissor_rect.bottom = render_ctx.height;


    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window * wnd = SDL_CreateWindow("LearningD3D12 SDL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, render_ctx.width, render_ctx.height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if(nullptr == wnd) {
        ::abort();
    }

    // Query Adapter (PhysicalDevice)
    IDXGIFactory * dxgi_factory = nullptr;
    CHECK_AND_FAIL(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgi_factory)));

    constexpr uint32_t MaxAdapters = 8;
    IDXGIAdapter * adapters[MaxAdapters] = {};
    IDXGIAdapter * pAdapter;
    for (UINT i = 0; dxgi_factory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        adapters[i] = pAdapter;
        DXGI_ADAPTER_DESC adapter_desc = {};
        ::printf("GPU Info [%d] :\n", i);
        if(SUCCEEDED(pAdapter->GetDesc(&adapter_desc))) {
            ::printf("\tDescription: %ls\n", adapter_desc.Description);
            ::printf("\tDedicatedVideoMemory: %zu\n", adapter_desc.DedicatedVideoMemory);
        }
    } // WARP -> Windows Advanced Rasterization ...

    // Create Logical Device
    auto res = D3D12CreateDevice(adapters[0], D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&render_ctx.device));
    CHECK_AND_FAIL(res);

    // Release adaptors
    for (unsigned i = 0; i < MaxAdapters; ++i) {
        if (adapters[i] != nullptr) {
            adapters[i]->Release();
        }
    }

    // Create Command Queues
    D3D12_COMMAND_QUEUE_DESC cmd_q_desc = {};
    cmd_q_desc.Type = D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT;
    cmd_q_desc.Flags = D3D12_COMMAND_QUEUE_FLAGS::D3D12_COMMAND_QUEUE_FLAG_NONE;
    res = render_ctx.device->CreateCommandQueue(&cmd_q_desc, IID_PPV_ARGS(&render_ctx.cmd_queue));
    CHECK_AND_FAIL(res);

    DXGI_MODE_DESC backbuffer_desc = {};
    backbuffer_desc.Width = render_ctx.width;
    backbuffer_desc.Height = render_ctx.height;
    backbuffer_desc.Format = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;

    DXGI_SAMPLE_DESC sampler_desc = {};
    sampler_desc.Count = 1;
    sampler_desc.Quality = 0;

    SDL_SysWMinfo wnd_info = {};
    SDL_GetWindowWMInfo(wnd, &wnd_info);
    HWND hwnd = wnd_info.info.win.window;

    // Create Swapchain
    DXGI_SWAP_CHAIN_DESC swapchain_desc = {};
    swapchain_desc.BufferDesc = backbuffer_desc;
    swapchain_desc.SampleDesc = sampler_desc;
    swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_desc.BufferCount = 2; // Use double-buffering;
    swapchain_desc.OutputWindow = hwnd;
    swapchain_desc.Windowed = TRUE;
    swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT::DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchain_desc.Flags = DXGI_SWAP_CHAIN_FLAG::DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    res = dxgi_factory->CreateSwapChain(render_ctx.cmd_queue, &swapchain_desc, &render_ctx.swapchain);
    CHECK_AND_FAIL(res);

    // -- to get current backbuffer index
    res = render_ctx.swapchain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&render_ctx.swapchain3);
    CHECK_AND_FAIL(res);
    render_ctx.frame_index = render_ctx.swapchain3->GetCurrentBackBufferIndex();
    ::printf("The current frame index is %d\n", render_ctx.frame_index);

    // Create Render Target View Descriptor Heap
    D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {};
    descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    descriptor_heap_desc.NumDescriptors = FRAME_COUNT;
    descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAGS::D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    res = render_ctx.device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&render_ctx.rtv_heap));
    CHECK_AND_FAIL(res);

    render_ctx.rtv_descriptor_size = render_ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    ::printf("size of rtv descriptor heap (to increment handle): %d\n", render_ctx.rtv_descriptor_size);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle_start = render_ctx.rtv_heap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < FRAME_COUNT; ++i) {
        res = render_ctx.swapchain3->GetBuffer(i, IID_PPV_ARGS(&render_ctx.render_targets[i]));
        CHECK_AND_FAIL(res);
        
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
        //cpu_handle.ptr = SIZE_T(INT64(rtv_handle_start) + INT64(1) * INT64(rtv_descriptor_size));
        cpu_handle.ptr = rtv_handle_start.ptr + ((UINT64)i * render_ctx.rtv_descriptor_size);
        render_ctx.device->CreateRenderTargetView(render_ctx.render_targets[i], nullptr, cpu_handle);
    }

    // Create command allocator
    res = render_ctx.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&render_ctx.cmd_allocator));
    CHECK_AND_FAIL(res);

    // Create empty root signature
    D3D12_ROOT_SIGNATURE_DESC root_desc = {};
    root_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ID3DBlob * signature = nullptr;
    ID3DBlob * signature_error_blob = nullptr;
    res = D3D12SerializeRootSignature(&root_desc, D3D_ROOT_SIGNATURE_VERSION::D3D_ROOT_SIGNATURE_VERSION_1, &signature, &signature_error_blob);
    CHECK_AND_FAIL(res);

    res = render_ctx.device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&render_ctx.root_signature));
    CHECK_AND_FAIL(res);

    // Load and compile shaders
    wchar_t const * shaders_path = L"./shaders/basic.hlsl";
    ID3DBlob * vertex_shader = nullptr;
    ID3DBlob * vs_err = nullptr;
    ID3DBlob * pixel_shader = nullptr;
    ID3DBlob * ps_err = nullptr;
    res = D3DCompileFromFile(shaders_path, nullptr, nullptr, "VSMain", "vs_5_0", compiler_flags, 0, &vertex_shader, &vs_err);
    if (FAILED(res)) {
        if (vs_err) {
            OutputDebugStringA((char *)vs_err->GetBufferPointer());
            vs_err->Release();
        } else {
            ::printf("could not load/compile shader\n");
        }
    }
    res = D3DCompileFromFile(shaders_path, nullptr, nullptr, "PSMain", "ps_5_0", compiler_flags, 0, &pixel_shader, &ps_err);
    if (FAILED(res)) {
        if (ps_err) {
            OutputDebugStringA((char *)ps_err->GetBufferPointer());
            ps_err->Release();
        }
    }

    // Create vertex-input-layout Elements
    D3D12_INPUT_ELEMENT_DESC input_desc [2];
    input_desc[0] = {};
    input_desc[0].SemanticName = "POSITION";
    input_desc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    input_desc[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    
    input_desc[1] = {};
    input_desc[1].SemanticName = "COLOR";
    input_desc[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    input_desc[1].AlignedByteOffset = 12; //?
    input_desc[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    // Create pipeline state object

    D3D12_BLEND_DESC blend_desc = {};
    blend_desc.AlphaToCoverageEnable                    = FALSE;
    blend_desc.IndependentBlendEnable                   = FALSE;
    blend_desc.RenderTarget[0].BlendEnable              = FALSE;
    blend_desc.RenderTarget[0].LogicOpEnable            = FALSE;
    blend_desc.RenderTarget[0].SrcBlend                 = D3D12_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlend                = D3D12_BLEND_ZERO;
    blend_desc.RenderTarget[0].BlendOp                  = D3D12_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].SrcBlendAlpha            = D3D12_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlendAlpha           = D3D12_BLEND_ZERO;
    blend_desc.RenderTarget[0].BlendOpAlpha             = D3D12_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].LogicOp                  = D3D12_LOGIC_OP_NOOP;
    blend_desc.RenderTarget[0].RenderTargetWriteMask    = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RASTERIZER_DESC rasterizer_desc = {};
    rasterizer_desc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer_desc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizer_desc.FrontCounterClockwise = false;
    rasterizer_desc.DepthClipEnable = TRUE;
    rasterizer_desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.pRootSignature = render_ctx.root_signature;
    pso_desc.VS.pShaderBytecode = vertex_shader->GetBufferPointer();
    pso_desc.VS.BytecodeLength = vertex_shader->GetBufferSize();
    pso_desc.PS.pShaderBytecode = pixel_shader->GetBufferPointer();
    pso_desc.PS.BytecodeLength = pixel_shader->GetBufferSize();
    pso_desc.BlendState = blend_desc;
    pso_desc.SampleMask = UINT_MAX;
    pso_desc.RasterizerState = rasterizer_desc;
    pso_desc.DepthStencilState.StencilEnable = FALSE;
    pso_desc.DepthStencilState.DepthEnable = FALSE;
    pso_desc.InputLayout.pInputElementDescs = input_desc;
    pso_desc.InputLayout.NumElements = _countof(input_desc);
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE::D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
    pso_desc.SampleDesc.Count = 1;
    pso_desc.SampleDesc.Quality = 0;

    res = render_ctx.device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&render_ctx.pso));
    CHECK_AND_FAIL(res);

    // Create command list
    res = render_ctx.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, render_ctx.cmd_allocator, render_ctx.pso, IID_PPV_ARGS(&render_ctx.direct_cmd_list));
    CHECK_AND_FAIL(res);

    // -- close command list for now (nothing to record yet)
    CHECK_AND_FAIL(render_ctx.direct_cmd_list->Close());

    // Create vertex buffer (VB)

    // vertex data
    Vertex vertices [3] = {};
    create_triangle_vertices(render_ctx.aspect_ratio, vertices);
    size_t vb_size = sizeof(vertices);

    // NOTE(omid): An upload heap is used here for code simplicity 
    //      and because there are very few verts to actually transfer.
    // NOTE(omid): using upload heaps to transfer static data such as vb(s) is not recommended.
    //      Every time the GPU needs it, the upload heap will be marshalled over. 
    //      Read up on Default Heap usage.

    D3D12_HEAP_PROPERTIES heap_props = {};
    heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap_props.CreationNodeMask = 1U;
    heap_props.VisibleNodeMask = 1U;

    D3D12_RESOURCE_DESC rsc_desc = {};
    rsc_desc.Dimension = D3D12_RESOURCE_DIMENSION::D3D12_RESOURCE_DIMENSION_BUFFER;
    rsc_desc.Width = vb_size;
    rsc_desc.Height = 1;
    rsc_desc.DepthOrArraySize = 1;
    rsc_desc.MipLevels = 1;
    rsc_desc.SampleDesc.Count = 1;
    rsc_desc.SampleDesc.Quality = 0;
    rsc_desc.Layout = D3D12_TEXTURE_LAYOUT::D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    res = render_ctx.device->CreateCommittedResource(
        &heap_props, D3D12_HEAP_FLAG_NONE, &rsc_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&render_ctx.vertex_buffer)
    );
    CHECK_AND_FAIL(res);

    // Copy vertex data to vertex buffer
    uint8_t * vertex_data = nullptr;
    D3D12_RANGE mem_range = {};
    mem_range.Begin = mem_range.End = 0; // We do not intend to read from this resource on the CPU.
    render_ctx.vertex_buffer->Map(0, &mem_range, reinterpret_cast<void **>(&vertex_data));
    memcpy(vertex_data, vertices, vb_size);
    render_ctx.vertex_buffer->Unmap(0, nullptr /*aka full-range*/);

    // Initialize the vertex buffer view (vbv)
    render_ctx.vb_view.BufferLocation = render_ctx.vertex_buffer->GetGPUVirtualAddress();
    render_ctx.vb_view.SizeInBytes = (UINT)vb_size;
    render_ctx.vb_view.StrideInBytes = sizeof(Vertex);

    // Create fence
    // create synchronization objects and wait until assets have been uploaded to the GPU.
    CHECK_AND_FAIL(render_ctx.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&render_ctx.fence)));

    render_ctx.fence_value = 1;

    // Create an event handle to use for frame synchronization.
    render_ctx.fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if(nullptr == render_ctx.fence_event) {
        // map the error code to an HRESULT value.
        res = HRESULT_FROM_WIN32(GetLastError());
        CHECK_AND_FAIL(res);
    }

    CHECK_AND_FAIL(wait_for_previous_frame(&render_ctx));

    // ========================================================================================================
    // -- Main loop
    SDL_Event e = {};
    bool running = true;
    while(running) {
        while(SDL_PollEvent(&e) != 0) {
            //User requests quit
            if(e.type == SDL_QUIT) {
                running = false;
            }
        }
        // OnUpdate()
        // -- nothing is updated
    
        // OnRender() aka rendering
        CHECK_AND_FAIL(render_triangle(&render_ctx));

        CHECK_AND_FAIL(wait_for_previous_frame(&render_ctx));
    }

    // ========================================================================================================
    // -- Cleanup
    CHECK_AND_FAIL(wait_for_previous_frame(&render_ctx));

    CloseHandle(render_ctx.fence_event);

    render_ctx.fence->Release();

    render_ctx.vertex_buffer->Release();

    render_ctx.direct_cmd_list->Release();
    render_ctx.pso->Release();

    pixel_shader->Release();
    vertex_shader->Release();

    render_ctx.root_signature->Release();
    if (signature_error_blob)
        signature_error_blob->Release();
    signature->Release();

    render_ctx.cmd_allocator->Release();

    for (unsigned i = 0; i < FRAME_COUNT; ++i) {
        render_ctx.render_targets[i]->Release();
    }

    render_ctx.rtv_heap->Release();

    render_ctx.swapchain3->Release();
    render_ctx.swapchain->Release();
    render_ctx.cmd_queue->Release();
    render_ctx.device->Release();
    dxgi_factory->Release();

    debug_interface_dx->Release();

    // -- advanced debugging and reporting live objects [from https://walbourn.github.io/dxgi-debug-device/]

    typedef HRESULT (WINAPI * LPDXGIGETDEBUGINTERFACE)(REFIID, void ** );

    //HMODULE dxgidebug_dll = LoadLibraryEx( L"dxgidebug_dll.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 );
    HMODULE dxgidebug_dll = LoadLibrary(L"DXGIDebug.dll");
    if (dxgidebug_dll) {
        auto dxgiGetDebugInterface = reinterpret_cast<LPDXGIGETDEBUGINTERFACE>(
            reinterpret_cast<void*>(GetProcAddress(dxgidebug_dll, "DXGIGetDebugInterface")));

        // -- working with dxgi_info_queue
        /*
        IDXGIInfoQueue * dxgiInfoQueue = nullptr;
        DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue)); 
        if (dxgiInfoQueue) {
            dxgiInfoQueue->SetBreakOnSeverity( DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true );
            dxgiInfoQueue->SetBreakOnSeverity( DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true );
        }
        */

        IDXGIDebug1 * dxgi_debugger = nullptr;
        DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgi_debugger));
        dxgi_debugger->ReportLiveObjects(
            DXGI_DEBUG_ALL,
            DXGI_DEBUG_RLO_DETAIL
            /* DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL) */
        );
        dxgi_debugger->Release();
        FreeLibrary(dxgidebug_dll);
    }

    SDL_DestroyWindow(wnd);
    SDL_Quit();
    
    return 0;
}

