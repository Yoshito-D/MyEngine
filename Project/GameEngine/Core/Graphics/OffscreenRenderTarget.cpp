#include "pch.h"
#include "OffscreenRenderTarget.h"
#include "GraphicsDevice.h"

namespace GameEngine {
void OffscreenRenderTarget::Initialize(GraphicsDevice* device, uint32_t width, uint32_t height) {
   width_ = width;
   height_ = height;
   device_ = device;

   // ポストエフェクトを連鎖させるため、入力と出力を交互に入れ替える2面を持つ。
   // スナップショットは透明描画が同時に書き込んでいるRTを自己参照しないための固定コピー。
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

   // SRV番号は描画コマンドやエフェクトがGPUハンドルとして保持し得るため再利用し、
   // サイズ依存のリソース本体とビュー内容だけを差し替える。
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

   // 初期状態をSRVにそろえることで、生成直後やSwapBuffers直後の面を入力として扱える。
   // 描画開始時だけPreDrawがRTVへ遷移させる。
   HRESULT hr = device_->GetDevice()->CreateCommittedResource(
	  &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
	  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue,
	  IID_PPV_ARGS(&info.renderTarget));
   assert(SUCCEEDED(hr));

   // RTV作成
   auto rtvHeapStart = device_->GetRTVHeap()->GetCPUDescriptorHandleForHeapStart();
   UINT rtvDescriptorSize = device_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

   info.rtvHandle = rtvHeapStart;
   // 0,1番はスワップチェーン用としてGraphicsDeviceが所有する。
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

   // 深度コピーではTYPELESS実体をR24_UNORM_X8_TYPELESSのSRVとして読むため、
   // リソース形式とビュー形式を分けて受け取る。
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
   // 呼び出し時点の描画状態を保存し、コピー後に同じ用途へ戻す。
   // 透明パス途中でも後続描画を継続できることが重要になる。
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
   // 同一サイズ・同一形式で作成しているため、サブリソース指定なしの全体コピーを使える。
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
   // 深度本体は通常DSVとして使用中なので、コピー元へ遷移させた区間だけ描画を止める。
   // コピー先をSRVへ戻してから本体もDEPTH_WRITEへ復帰させる。
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

   // ping-pongで直前まで入力だった面を、次のエフェクトの出力先へ切り替える。
   if (currentRenderTarget_.state != D3D12_RESOURCE_STATE_RENDER_TARGET) {
	  CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		 currentRenderTarget_.renderTarget.Get(),
		 currentRenderTarget_.state,
		 D3D12_RESOURCE_STATE_RENDER_TARGET);
	  commandList->ResourceBarrier(1, &barrier);
	  currentRenderTarget_.state = D3D12_RESOURCE_STATE_RENDER_TARGET;
   }

   // シーン描画ではDSVもクリアするが、色だけを処理するポストエフェクトでは
   // 深度を束縛せず、別途保存した深度SRVとの競合を避ける。
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

   // ポストプロセス済みの色へUI等を重ねる経路なので、RTV状態へ戻しても色は消去しない。
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

   // 次のポストエフェクトまたは最終合成がこの面を読むため、書き込み完了後にSRVへ戻す。
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
