namespace Gui
{
namespace Priv
{

struct TFrameResources
{
    ID3D12Resource* VertexBuffer;
    void* VertexBufferCpuAddress;
    unsigned VertexBufferSize;
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView;

    ID3D12Resource* IndexBuffer;
    void* IndexBufferCpuAddress;
    unsigned IndexBufferSize;
    D3D12_INDEX_BUFFER_VIEW IndexBufferView;
};

static TFrameResources GFrameResources[2];
static ID3D12RootSignature* GRootSignature;
static ID3D12PipelineState* GPipelineState;
static ID3D12Resource* GFontTexture;
static D3D12_CPU_DESCRIPTOR_HANDLE GFontTextureDescriptor;

} // namespace Priv

static void
Initialize()
{
    ImGuiIO& Io = ImGui::GetIO();
    Io.KeyMap[ImGuiKey_Tab] = VK_TAB;
    Io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
    Io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
    Io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
    Io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
    Io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
    Io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
    Io.KeyMap[ImGuiKey_Home] = VK_HOME;
    Io.KeyMap[ImGuiKey_End] = VK_END;
    Io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
    Io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
    Io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
    Io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
    Io.KeyMap[ImGuiKey_A] = 'A';
    Io.KeyMap[ImGuiKey_C] = 'C';
    Io.KeyMap[ImGuiKey_V] = 'V';
    Io.KeyMap[ImGuiKey_X] = 'X';
    Io.KeyMap[ImGuiKey_Y] = 'Y';
    Io.KeyMap[ImGuiKey_Z] = 'Z';
    Io.ImeWindowHandle = Dx::GWindow;
    Io.RenderDrawListsFn = nullptr;
    Io.DisplaySize = ImVec2((float)Dx::GResolution[0], (float)Dx::GResolution[1]);
    ImGui::GetStyle().WindowRounding = 0.0f;

    uint8_t* Pixels;
    int Width, Height;
    ImGui::GetIO().Fonts->AddFontFromFileTTF("Data/Roboto-Medium.ttf", 18.0f);
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&Pixels, &Width, &Height);

    const auto TextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, (UINT64)Width, Height);
    VHR(Dx::GDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
                                             &TextureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                             IID_PPV_ARGS(&Priv::GFontTexture)));

    ID3D12Resource* IntermediateBuffer = nullptr;
    {
        uint64_t BufferSize;
        Dx::GDevice->GetCopyableFootprints(&TextureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &BufferSize);

        const auto BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(BufferSize);
        VHR(Dx::GDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
                                                 &BufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                 IID_PPV_ARGS(&IntermediateBuffer)));

        Dx::GIntermediateResources.push_back(IntermediateBuffer);
    }

    D3D12_SUBRESOURCE_DATA TextureData = { Pixels, (LONG_PTR)(Width * 4) };
    UpdateSubresources<1>(Dx::GCmdList, Priv::GFontTexture, IntermediateBuffer, 0, 0, 1, &TextureData);

    Dx::GCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(Priv::GFontTexture,
                                                                           D3D12_RESOURCE_STATE_COPY_DEST,
                                                                           D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
    D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
    SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    SrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    SrvDesc.Texture2D.MipLevels = 1;

    Dx::AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, Priv::GFontTextureDescriptor);
    Dx::GDevice->CreateShaderResourceView(Priv::GFontTexture, &SrvDesc, Priv::GFontTextureDescriptor);


    D3D12_INPUT_ELEMENT_DESC InputElements[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    std::vector<uint8_t> CsoVs = Lib::LoadFile("Data/Shaders/Gui.vs.cso");
    std::vector<uint8_t> CsoPs = Lib::LoadFile("Data/Shaders/Gui.ps.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC PsoDesc = {};
    PsoDesc.InputLayout = { InputElements, (unsigned)std::size(InputElements) };
    PsoDesc.VS = { CsoVs.data(), CsoVs.size() };
    PsoDesc.PS = { CsoPs.data(), CsoPs.size() };
    PsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    PsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    PsoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    PsoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    PsoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    PsoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    PsoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    PsoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    PsoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    PsoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    PsoDesc.SampleMask = UINT_MAX;
    PsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    PsoDesc.NumRenderTargets = 1;
    PsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    PsoDesc.SampleDesc.Count = 1;

    VHR(Dx::GDevice->CreateGraphicsPipelineState(&PsoDesc, IID_PPV_ARGS(&Priv::GPipelineState)));
    VHR(Dx::GDevice->CreateRootSignature(0, CsoVs.data(), CsoVs.size(), IID_PPV_ARGS(&Priv::GRootSignature)));
}

static void
Shutdown()
{
    // @Incomplete: Release all resources.
}

static void
Update(float DeltaTime)
{
    ImGuiIO& Io = ImGui::GetIO();
    Io.KeyCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    Io.KeyShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    Io.KeyAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    Io.DeltaTime = DeltaTime;
}

static void
Render()
{
    ImDrawData* DrawData = ImGui::GetDrawData();
    if (!DrawData || DrawData->TotalVtxCount == 0)
        return;

    ImGuiIO& Io = ImGui::GetIO();
    Priv::TFrameResources& Frame = Priv::GFrameResources[Dx::GFrameIndex];

    const int ViewportWidth = (int)(Io.DisplaySize.x * Io.DisplayFramebufferScale.x);
    const int ViewportHeight = (int)(Io.DisplaySize.y * Io.DisplayFramebufferScale.y);
    DrawData->ScaleClipRects(Io.DisplayFramebufferScale);

    // create/resize vertex buffer
    if (Frame.VertexBufferSize == 0 || Frame.VertexBufferSize < DrawData->TotalVtxCount * sizeof(ImDrawVert))
    {
        SAFE_RELEASE(Frame.VertexBuffer);
        VHR(Dx::GDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
                                                 &CD3DX12_RESOURCE_DESC::Buffer(DrawData->TotalVtxCount * sizeof(ImDrawVert)),
                                                 D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                 IID_PPV_ARGS(&Frame.VertexBuffer)));

        VHR(Frame.VertexBuffer->Map(0, &CD3DX12_RANGE(0, 0), &Frame.VertexBufferCpuAddress));

        Frame.VertexBufferSize = DrawData->TotalVtxCount * sizeof(ImDrawVert);

        Frame.VertexBufferView.BufferLocation = Frame.VertexBuffer->GetGPUVirtualAddress();
        Frame.VertexBufferView.StrideInBytes = sizeof(ImDrawVert);
        Frame.VertexBufferView.SizeInBytes = DrawData->TotalVtxCount * sizeof(ImDrawVert);
    }

    // create/resize index buffer
    if (Frame.IndexBufferSize == 0 || Frame.IndexBufferSize < DrawData->TotalIdxCount * sizeof(ImDrawIdx))
    {
        SAFE_RELEASE(Frame.IndexBuffer);
        VHR(Dx::GDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
                                                 &CD3DX12_RESOURCE_DESC::Buffer(DrawData->TotalIdxCount * sizeof(ImDrawIdx)),
                                                 D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                 IID_PPV_ARGS(&Frame.IndexBuffer)));

        VHR(Frame.IndexBuffer->Map(0, &CD3DX12_RANGE(0, 0), &Frame.IndexBufferCpuAddress));

        Frame.IndexBufferSize = DrawData->TotalIdxCount * sizeof(ImDrawIdx);

        Frame.IndexBufferView.BufferLocation = Frame.IndexBuffer->GetGPUVirtualAddress();
        Frame.IndexBufferView.Format = sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
        Frame.IndexBufferView.SizeInBytes = DrawData->TotalIdxCount * sizeof(ImDrawIdx);
    }

    // update vertex and index buffers
    {
        ImDrawVert* VertexPtr = (ImDrawVert*)Frame.VertexBufferCpuAddress;
        ImDrawIdx* IndexPtr = (ImDrawIdx*)Frame.IndexBufferCpuAddress;

        for (unsigned N = 0; N < (unsigned)DrawData->CmdListsCount; ++N)
        {
            ImDrawList* DrawList = DrawData->CmdLists[N];
            memcpy(VertexPtr, &DrawList->VtxBuffer[0], DrawList->VtxBuffer.size() * sizeof(ImDrawVert));
            memcpy(IndexPtr, &DrawList->IdxBuffer[0], DrawList->IdxBuffer.size() * sizeof(ImDrawIdx));
            VertexPtr += DrawList->VtxBuffer.size();
            IndexPtr += DrawList->IdxBuffer.size();
        }
    }

    D3D12_GPU_VIRTUAL_ADDRESS ConstantBufferGpuAddress;
    void* ConstantBufferCpuAddress = Dx::AllocateGpuUploadMemory(64, ConstantBufferGpuAddress);

    // update constant buffer
    {
        XMMATRIX M = XMMatrixTranspose(XMMatrixOrthographicOffCenterLH(0.0f, (float)ViewportWidth,
                                                                       (float)ViewportHeight, 0.0f,
                                                                       0.0f, 1.0f));
        XMFLOAT4X4A F;
        XMStoreFloat4x4A(&F, M);
        memcpy(ConstantBufferCpuAddress, &F, sizeof(F));
    }

    D3D12_VIEWPORT Viewport = { 0.0f, 0.0f, (float)ViewportWidth, (float)ViewportHeight, 0.0f, 1.0f };
    Dx::GCmdList->RSSetViewports(1, &Viewport);

    Dx::GCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Dx::GCmdList->SetPipelineState(Priv::GPipelineState);

    Dx::GCmdList->SetGraphicsRootSignature(Priv::GRootSignature);
    Dx::GCmdList->SetGraphicsRootConstantBufferView(0, ConstantBufferGpuAddress);
    Dx::GCmdList->SetGraphicsRootDescriptorTable(1, Dx::CopyDescriptorsToGpu(1, Priv::GFontTextureDescriptor));

    Dx::GCmdList->IASetVertexBuffers(0, 1, &Frame.VertexBufferView);
    Dx::GCmdList->IASetIndexBuffer(&Frame.IndexBufferView);


    int VertexOffset = 0;
    unsigned IndexOffset = 0;
    for (unsigned N = 0; N < (unsigned)DrawData->CmdListsCount; ++N)
    {
        ImDrawList* DrawList = DrawData->CmdLists[N];

        for (unsigned CmdIndex = 0; CmdIndex < (uint32_t)DrawList->CmdBuffer.size(); ++CmdIndex)
        {
            ImDrawCmd* Cmd = &DrawList->CmdBuffer[CmdIndex];

            if (Cmd->UserCallback)
            {
                Cmd->UserCallback(DrawList, Cmd);
            }
            else
            {
                D3D12_RECT R = { (LONG)Cmd->ClipRect.x, (LONG)Cmd->ClipRect.y, (LONG)Cmd->ClipRect.z, (LONG)Cmd->ClipRect.w };
                Dx::GCmdList->RSSetScissorRects(1, &R);
                Dx::GCmdList->DrawIndexedInstanced(Cmd->ElemCount, 1, IndexOffset, VertexOffset, 0);
            }
            IndexOffset += Cmd->ElemCount;
        }
        VertexOffset += DrawList->VtxBuffer.size();
    }
}

} // namespace Gui
// vim: set ts=4 sw=4 expandtab:
