#include "pch.h"
#include "OffscreenRenderTarget.h"
#include "GraphicsDevice.h"

namespace GameEngine {
void OffscreenRenderTarget::Initialize(GraphicsDevice* device, uint32_t width, uint32_t height) {
   width_ = width;
   height_ = height;
   device_ = device;

   // 2つのレンダーターゲットを作成
   currentRenderTarget_ = CreateRenderTargetInfo(0);
   previousRenderTarget_ = CreateRenderTargetInfo(1);
   sceneColorSnapshot_ = CreateSnapshotInfo(format_, format_);
   sceneDepthSnapshot_ = CreateSnapshotInfo(DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
}

void OffscreenRenderTarget::Resize(uint32_t width, uint32_t height) {
   if (!device_ || width == 0 || height == 0) {
	  return;
   }

   if (width_ == width && height_ == height) {
	  return;
   }

   const UINT currentSrvIndex = currentRenderTarget_.srvIndex;
   const UINT previousSrvIndex = previousRenderTarget_.srvIndex;
   const UINT sceneColorSrvIndex = sceneColorSnapshot_.srvIndex;
   const UINT sceneDepthSrvIndex = sceneDepthSnapshot_.srvIndex;

   currentRenderTarget_.renderTarget.Reset();
   previousRenderTarget_.renderTarget.Reset();
   sceneColorSnapshot_.renderTarget.Reset();
   sceneDepthSnapshot_.renderTarget.Reset();

   width_ = width;
   height_ = height;

   currentRenderTarget_ = CreateRenderTargetInfo(0, currentSrvIndex);
   previousRenderTarget_ = CreateRenderTargetInfo(1, previousSrvIndex);
   sceneColorSnapshot_ = CreateSnapshotInfo(format_, format_, sceneColorSrvIndex);
   sceneDepthSnapshot_ = CreateSnapshotInfo(DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, sceneDepthSrvIndex);
}

OffscreenRenderTarget::RenderTargetInfo OffscreenRenderTarget::CreateRenderTargetInfo(int index, UINT srvIndex) {
   RenderTargetInfo info;

   D3D12_RESOURCE_DESC texDesc = {};
   texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   texDesc.Width = width_;
   texDesc.Height = height_;
   texDesc.DepthOrArraySize = 1;
   texDesc.MipLevels = 1;
   texDesc.Format = format_;
   texDesc.SampleDesc.Count = 1;
   texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
   texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

   D3D12_CLEAR_VALUE clearValue = {};
   clearValue.Format = texDesc.Format;
   memcpy(clearValue.Color, clearColor_, sizeof(clearColor_));

   D3D12_HEAP_PROPERTIES heapProps = {};
   heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

   // レンダーターゲットリソースを作成
   HRESULT hr = device_->GetDevice()->CreateCommittedResource(
	  &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
	  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue,
	  IID_PPV_ARGS(&info.renderTarget));
   assert(SUCCEEDED(hr));

   // RTV作成
   auto rtvHeapStart = device_->GetRTVHeap()->GetCPUDescriptorHandleForHeapStart();
   UINT rtvDescriptorSize = device_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

   info.rtvHandle = rtvHeapStart;
   info.rtvHandle.ptr += rtvDescriptorSize * (2 + index);  // RTV heap内の位置（例：2,3番目）

   device_->GetDevice()->CreateRenderTargetView(info.renderTarget.Get(), nullptr, info.rtvHandle);

   // SRV作成
   const bool allocateSrvIndex = srvIndex == static_cast<UINT>(-1);
   if (allocateSrvIndex) {
	  srvIndex = device_->GetNextSrvIndex();
   }
   info.srvIndex = srvIndex;
   info.srvHandleCPU = CD3DX12_CPU_DESCRIPTOR_HANDLE(
	  device_->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart(), srvIndex,
	  device_->GetDescriptorSizeCBVSRVUAV());
   info.srvHandleGPU = CD3DX12_GPU_DESCRIPTOR_HANDLE(
	  device_->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart(), srvIndex,
	  device_->GetDescriptorSizeCBVSRVUAV());

   D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
   srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   srvDesc.Format = format_;
   srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
   srvDesc.Texture2D.MipLevels = 1;

   device_->GetDevice()->CreateShaderResourceView(info.renderTarget.Get(), &srvDesc, info.srvHandleCPU);
   if (allocateSrvIndex) {
	  device_->IncrementSrvIndex();
   }

   return info;
}

OffscreenRenderTarget::RenderTargetInfo OffscreenRenderTarget::CreateSnapshotInfo(
   DXGI_FORMAT resourceFormat, DXGI_FORMAT srvFormat, UINT srvIndex) {
   RenderTargetInfo info;

   D3D12_RESOURCE_DESC textureDesc{};
   textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   textureDesc.Width = width_;
   textureDesc.Height = height_;
   textureDesc.DepthOrArraySize = 1;
   textureDesc.MipLevels = 1;
   textureDesc.Format = resourceFormat;
   textureDesc.SampleDesc.Count = 1;
   textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
   textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

   D3D12_HEAP_PROPERTIES heapProperties{};
   heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
   const HRESULT result = device_->GetDevice()->CreateCommittedResource(
	  &heapProperties,
	  D3D12_HEAP_FLAG_NONE,
	  &textureDesc,
	  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
	  nullptr,
	  IID_PPV_ARGS(&info.renderTarget));
   assert(SUCCEEDED(result));

   const bool allocateSrvIndex = srvIndex == static_cast<UINT>(-1);
   if (allocateSrvIndex) {
	  srvIndex = device_->GetNextSrvIndex();
   }
   info.srvIndex = srvIndex;
   info.srvHandleCPU = CD3DX12_CPU_DESCRIPTOR_HANDLE(
	  device_->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart(),
	  srvIndex,
	  device_->GetDescriptorSizeCBVSRVUAV());
   info.srvHandleGPU = CD3DX12_GPU_DESCRIPTOR_HANDLE(
	  device_->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart(),
	  srvIndex,
	  device_->GetDescriptorSizeCBVSRVUAV());

   D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
   srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   srvDesc.Format = srvFormat;
   srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
   srvDesc.Texture2D.MipLevels = 1;
   device_->GetDevice()->CreateShaderResourceView(info.renderTarget.Get(), &srvDesc, info.srvHandleCPU);
   if (allocateSrvIndex) {
	  device_->IncrementSrvIndex();
   }
   info.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
   return info;
}

void OffscreenRenderTarget::CaptureSceneTextures() {
   CaptureSceneColor();
   CaptureSceneDepth();
}

void OffscreenRenderTarget::CaptureSceneColor() {
   if (!device_ || !currentRenderTarget_.renderTarget || !sceneColorSnapshot_.renderTarget) {
	  return;
   }

   auto* commandList = device_->GetCommandList();
   const D3D12_RESOURCE_STATES previousColorState = currentRenderTarget_.state;
   if (previousColorState != D3D12_RESOURCE_STATE_COPY_SOURCE) {
	  const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		 currentRenderTarget_.renderTarget.Get(), previousColorState, D3D12_RESOURCE_STATE_COPY_SOURCE);
	  commandList->ResourceBarrier(1, &barrier);
   }
   {
	  const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		 sceneColorSnapshot_.renderTarget.Get(), sceneColorSnapshot_.state, D3D12_RESOURCE_STATE_COPY_DEST);
	  commandList->ResourceBarrier(1, &barrier);
	  sceneColorSnapshot_.state = D3D12_RESOURCE_STATE_COPY_DEST;
   }
   commandList->CopyResource(sceneColorSnapshot_.renderTarget.Get(), currentRenderTarget_.renderTarget.Get());
   {
	  const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		 sceneColorSnapshot_.renderTarget.Get(), sceneColorSnapshot_.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	  commandList->ResourceBarrier(1, &barrier);
	  sceneColorSnapshot_.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
   }
   if (previousColorState != D3D12_RESOURCE_STATE_COPY_SOURCE) {
	  const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		 currentRenderTarget_.renderTarget.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, previousColorState);
	  commandList->ResourceBarrier(1, &barrier);
   }
}

void OffscreenRenderTarget::CaptureSceneDepth() {
   if (!device_ || !sceneDepthSnapshot_.renderTarget || !device_->GetDepthBufferResource()) {
	  return;
   }

   auto* commandList = device_->GetCommandList();
   device_->TransitionDepthStencilToCopySource();
   {
	  const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		 sceneDepthSnapshot_.renderTarget.Get(), sceneDepthSnapshot_.state, D3D12_RESOURCE_STATE_COPY_DEST);
	  commandList->ResourceBarrier(1, &barrier);
	  sceneDepthSnapshot_.state = D3D12_RESOURCE_STATE_COPY_DEST;
   }
   commandList->CopyResource(sceneDepthSnapshot_.renderTarget.Get(), device_->GetDepthBufferResource());
   {
	  const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		 sceneDepthSnapshot_.renderTarget.Get(), sceneDepthSnapshot_.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	  commandList->ResourceBarrier(1, &barrier);
	  sceneDepthSnapshot_.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
   }
   device_->TransitionDepthStencilToWrite();
}

void OffscreenRenderTarget::PreDraw(bool useDSV) {
   auto commandList = device_->GetCommandList();

   // バリア：SRV -> RTV
   if (currentRenderTarget_.state != D3D12_RESOURCE_STATE_RENDER_TARGET) {
	  CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		 currentRenderTarget_.renderTarget.Get(),
		 currentRenderTarget_.state,
		 D3D12_RESOURCE_STATE_RENDER_TARGET);
	  commandList->ResourceBarrier(1, &barrier);
	  currentRenderTarget_.state = D3D12_RESOURCE_STATE_RENDER_TARGET;
   }

   // 描画ターゲットを設定
   if (useDSV) {
	  device_->TransitionDepthStencilToWrite();
	  CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(device_->GetDSVHeap()->GetCPUDescriptorHandleForHeapStart());
	  commandList->OMSetRenderTargets(1, &currentRenderTarget_.rtvHandle, FALSE, &dsvHandle);

	  // DSVクリア（重要：深度バッファを確実にクリア）
	  commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
   } else {
	  commandList->OMSetRenderTargets(1, &currentRenderTarget_.rtvHandle, FALSE, nullptr);
   }

   // RTVクリア
   commandList->ClearRenderTargetView(currentRenderTarget_.rtvHandle, clearColor_, 0, nullptr);

   // ビューポートとシザー設定
   CD3DX12_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_));
   commandList->RSSetViewports(1, &viewport);

   CD3DX12_RECT scissorRect(0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_));
   commandList->RSSetScissorRects(1, &scissorRect);

   // SRVヒープの設定
   Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeaps[] = { device_->GetSRVHeap() };
   commandList->SetDescriptorHeaps(1, descriptorHeaps->GetAddressOf());
}

void OffscreenRenderTarget::PreDrawWithoutClear(bool useDSV) {
   auto commandList = device_->GetCommandList();

   // バリア：SRV -> RTV
   if (currentRenderTarget_.state != D3D12_RESOURCE_STATE_RENDER_TARGET) {
	  CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		 currentRenderTarget_.renderTarget.Get(),
		 currentRenderTarget_.state,
		 D3D12_RESOURCE_STATE_RENDER_TARGET);
	  commandList->ResourceBarrier(1, &barrier);
	  currentRenderTarget_.state = D3D12_RESOURCE_STATE_RENDER_TARGET;
   }

   if (useDSV) {
	  device_->TransitionDepthStencilToWrite();
	  CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(device_->GetDSVHeap()->GetCPUDescriptorHandleForHeapStart());
	  commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	  // DSVクリア
	  commandList->OMSetRenderTargets(1, &currentRenderTarget_.rtvHandle, FALSE, &dsvHandle);
   } else {
	  commandList->OMSetRenderTargets(1, &currentRenderTarget_.rtvHandle, FALSE, nullptr);
   }

   // ビューポートとシザー設定
   CD3DX12_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_));
   commandList->RSSetViewports(1, &viewport);

   CD3DX12_RECT scissorRect(0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_));
   commandList->RSSetScissorRects(1, &scissorRect);

   // SRVヒープの設定
   Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeaps[] = { device_->GetSRVHeap() };
   commandList->SetDescriptorHeaps(1, descriptorHeaps->GetAddressOf());
}

void OffscreenRenderTarget::PostDraw() {
   auto commandList = device_->GetCommandList();

   // バリア：RTV -> SRV
   if (currentRenderTarget_.state != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
	  CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		 currentRenderTarget_.renderTarget.Get(),
		 currentRenderTarget_.state,
		 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	  commandList->ResourceBarrier(1, &barrier);
	  currentRenderTarget_.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
   }
}
}
