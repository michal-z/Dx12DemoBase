#include "External.h"
#include "Demo.h"


static void
BeginFrame()
{
    ID3D12CommandAllocator* CmdAlloc = Dx::GCmdAlloc[Dx::GFrameIndex];
    Dx::TGraphicsCommandList* CmdList = Dx::GCmdList;

    CmdAlloc->Reset();
    CmdList->Reset(CmdAlloc, nullptr);

    Dx::SetDescriptorHeap();

    CmdList->RSSetViewports(1, &CD3DX12_VIEWPORT(0.0f, 0.0f, (float)Dx::GResolution[0], (float)Dx::GResolution[1]));
    CmdList->RSSetScissorRects(1, &CD3DX12_RECT(0, 0, Dx::GResolution[0], Dx::GResolution[1]));

    ID3D12Resource* BackBuffer;
    D3D12_CPU_DESCRIPTOR_HANDLE BackBufferHandle;
    Dx::GetBackBuffer(BackBuffer, BackBufferHandle);

    CmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(BackBuffer, D3D12_RESOURCE_STATE_PRESENT,
                                                                      D3D12_RESOURCE_STATE_RENDER_TARGET));

    Dx::GCmdList->OMSetRenderTargets(1, &BackBufferHandle, FALSE, &Dx::GDepthBufferHandle);
    Dx::GCmdList->ClearRenderTargetView(BackBufferHandle, XMVECTORF32{ 1.0f, 1.0f, 1.0f, 0.0f }, 0, nullptr);
    Dx::GCmdList->ClearDepthStencilView(Dx::GDepthBufferHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

static void
EndFrame()
{
    ID3D12Resource* BackBuffer;
    D3D12_CPU_DESCRIPTOR_HANDLE BackBufferHandle;
    Dx::GetBackBuffer(BackBuffer, BackBufferHandle);

    Dx::GCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(BackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                                           D3D12_RESOURCE_STATE_PRESENT));
    VHR(Dx::GCmdList->Close());

    Dx::GCmdQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&Dx::GCmdList);
}

static void
Update(double Time, float DeltaTime)
{
    ImGui::ShowDemoWindow();
}

static void
Initialize()
{
}

static void
Shutdown()
{
}

int CALLBACK
WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    const char* WindowName = "Demo1";
    const unsigned WindowWidth = 1920;
    const unsigned WindowHeight = 1080;

    SetProcessDPIAware();
    ImGui::CreateContext();

    Dx::Initialize(Lib::InitializeWindow(WindowName, WindowWidth, WindowHeight));
    Gui::Initialize();
    Initialize();

    // Upload resources to the GPU.
    VHR(Dx::GCmdList->Close());
    Dx::GCmdQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&Dx::GCmdList);
    Dx::WaitForGpu();

    for (ID3D12Resource* Resource : Dx::GIntermediateResources)
        SAFE_RELEASE(Resource);

    Dx::GIntermediateResources.clear();


    for (;;)
    {
        MSG Message = {};
        if (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        {
            DispatchMessage(&Message);
            if (Message.message == WM_QUIT)
                break;
        }
        else
        {
            double Time;
            float DeltaTime;
            Lib::UpdateFrameStats(Dx::GWindow, WindowName, Time, DeltaTime);
            Gui::Update(DeltaTime);

            BeginFrame();
            ImGui::NewFrame();
            Update(Time, DeltaTime);
            ImGui::Render();
            Gui::Render();
            EndFrame();
            Dx::PresentFrame();
        }
    }

    Dx::WaitForGpu();
    Shutdown();
    Gui::Shutdown();
    Dx::Shutdown();
    return 0;
}

#include "Directx12.cpp"
#include "Gui.cpp"
#include "Library.cpp"
// vim: set ts=4 sw=4 expandtab:
