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
}

OffscreenRenderTarget::RenderTargetInfo OffscreenRenderTarget::CreateRenderTargetInfo(int index) {
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
   UINT srvIndex = device_->GetNextSrvIndex();
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
   device_->IncrementSrvIndex();

   return info;
}

void OffscreenRenderTarget::PreDraw(bool useDSV) {
   auto commandList = device_->GetCommandList();

   // バリア：SRV -> RTV
   CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
	  currentRenderTarget_.renderTarget.Get(),
	  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
	  D3D12_RESOURCE_STATE_RENDER_TARGET);
   commandList->ResourceBarrier(1, &barrier);

   // 描画ターゲットを設定
   if (useDSV) {
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
   CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
	  currentRenderTarget_.renderTarget.Get(),
	  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
	  D3D12_RESOURCE_STATE_RENDER_TARGET);
   commandList->ResourceBarrier(1, &barrier);

   if (useDSV) {
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
   CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
	  currentRenderTarget_.renderTarget.Get(),
	  D3D12_RESOURCE_STATE_RENDER_TARGET,
	  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
   commandList->ResourceBarrier(1, &barrier);
}
}