#include "pch.h"
#include "GraphicsDevice.h"
#include "ImGuiManager.h"
#include <wincodec.h>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

namespace GameEngine {
namespace {
constexpr UINT kShaderVisibleDescriptorCount = 4096;
constexpr float kBackBufferClearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
constexpr int64_t kMicrosecondsPerSecond = 1'000'000;
constexpr int64_t kTargetFramesPerSecond = 60;
constexpr int64_t kSleepCheckFramesPerSecond = 65;
constexpr std::chrono::microseconds kTargetFrameTime(
   kMicrosecondsPerSecond / kTargetFramesPerSecond);
constexpr std::chrono::microseconds kSleepCheckTime(
   kMicrosecondsPerSecond / kSleepCheckFramesPerSecond);
constexpr std::chrono::microseconds kSpinSleepInterval(1);
}

void GraphicsDevice::Initialize(Window* window, int32_t backBufferWidth, int32_t backBufferHeight, bool enableDebugLayer) {
   window_ = window;
   backBufferWidth_ = backBufferWidth;
   backBufferHeight_ = backBufferHeight;

   // 後続の生成処理は前段で得たデバイス、キュー、各ヒープを参照するため、
   // 依存関係の順に初期化する。特にDSVはシェーダー参照用SRVも同時に作る。
   InitializeDXGIDevice(enableDebugLayer);

   InitializeCommand();

   InitializeFixFPS();

   CreateSwapChain();

   CreateRenderTargetViews();

   CreateSRVHeap();

   CreateDepthStencilViews();

   CreateFence();
}

void GraphicsDevice::PreDraw() {
   // Present済みのバッファへ直接書き込めないため、このフレームが所有する
   // バックバッファだけをRENDER_TARGETへ遷移させる。
   UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

   // リソースバリアを変更（表示状態→描画対象）
   CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
	  backBuffers_[backBufferIndex].Get(), D3D12_RESOURCE_STATE_PRESENT,
	  D3D12_RESOURCE_STATE_RENDER_TARGET);
   commandList_->ResourceBarrier(1, &barrier);

   UINT rtvDescriptorSize = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

   // レンダーターゲットビュー用ディスクリプタヒープのハンドルを取得
   CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap_->GetCPUDescriptorHandleForHeapStart());
   rtvHandle.Offset(backBufferIndex, rtvDescriptorSize); // バックバッファごとのRTVのオフセット

   CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvHeap_->GetCPUDescriptorHandleForHeapStart());

   TransitionDepthStencilToWrite();

   // RTVとDSVを同時に束縛してからクリアする。DSVはポストエフェクトでSRV化されるため、
   // フレーム開始時に必ず書き込み状態へ戻しておく。
   commandList_->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

   // 指定した深度で画面全体をクリアする
   commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

   // 指定した色で画面全体をクリアする
   commandList_->ClearRenderTargetView(rtvHandle, kBackBufferClearColor, 0, nullptr);

   // ビューポートの設定
   CD3DX12_VIEWPORT viewport =
	  CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(backBufferWidth_), static_cast<float>(backBufferHeight_));
   commandList_->RSSetViewports(1, &viewport);
   // シザリング矩形の設定
   CD3DX12_RECT scissorRect = CD3DX12_RECT(0, 0, backBufferWidth_, backBufferHeight_);
   commandList_->RSSetScissorRects(1, &scissorRect);

   // シェーダー可視ヒープはコマンドリスト全体で一つだけを共有する。
   // 描画側はこのヒープを前提にGPUディスクリプタハンドルを設定する。
   ComPtr<ID3D12DescriptorHeap> descriptorHeaps[] = { srvHeap_ };
   commandList_->SetDescriptorHeaps(1, descriptorHeaps->GetAddressOf());
}

void GraphicsDevice::RequestScreenshot(const std::filesystem::path& outputPath) {
   if (!outputPath.empty()) {
      screenshotRequests_.push(outputPath);
   }
}

void GraphicsDevice::PostDraw() {
   HRESULT result = S_FALSE;

   const UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
   ID3D12Resource* backBuffer = backBuffers_[backBufferIndex].Get();
   Microsoft::WRL::ComPtr<ID3D12Resource> screenshotReadback;
   std::optional<std::filesystem::path> screenshotPath;
   D3D12_PLACED_SUBRESOURCE_FOOTPRINT screenshotFootprint{};
   UINT64 screenshotBufferSize = 0;
   D3D12_RESOURCE_DESC screenshotSourceDesc{};

   if (!screenshotRequests_.empty()) {
      // GPUテクスチャは行ピッチに配置制約があるため、単純なwidth * pixelSizeではなく
      // GetCopyableFootprintsが返すレイアウトでリードバックバッファを確保する。
      screenshotPath = screenshotRequests_.front();
      screenshotRequests_.pop();
      screenshotSourceDesc = backBuffer->GetDesc();
      device_->GetCopyableFootprints(
         &screenshotSourceDesc,
         0,
         1,
         0,
         &screenshotFootprint,
         nullptr,
         nullptr,
         &screenshotBufferSize);

      const CD3DX12_HEAP_PROPERTIES readbackHeap(D3D12_HEAP_TYPE_READBACK);
      const CD3DX12_RESOURCE_DESC readbackBuffer =
         CD3DX12_RESOURCE_DESC::Buffer(screenshotBufferSize);
      result = device_->CreateCommittedResource(
         &readbackHeap,
         D3D12_HEAP_FLAG_NONE,
         &readbackBuffer,
         D3D12_RESOURCE_STATE_COPY_DEST,
         nullptr,
         IID_PPV_ARGS(screenshotReadback.GetAddressOf()));

      if (SUCCEEDED(result)) {
         // バックバッファを一時的にコピー元へ変更し、コピー後はPresent可能な状態へ戻す。
         // スクリーンショットの有無で最終状態が変わらないことが後段のPresentの前提となる。
         const CD3DX12_RESOURCE_BARRIER toCopySource =
            CD3DX12_RESOURCE_BARRIER::Transition(
               backBuffer,
               D3D12_RESOURCE_STATE_RENDER_TARGET,
               D3D12_RESOURCE_STATE_COPY_SOURCE);
         commandList_->ResourceBarrier(1, &toCopySource);

         D3D12_TEXTURE_COPY_LOCATION source{};
         source.pResource = backBuffer;
         source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
         source.SubresourceIndex = 0;
         D3D12_TEXTURE_COPY_LOCATION destination{};
         destination.pResource = screenshotReadback.Get();
         destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
         destination.PlacedFootprint = screenshotFootprint;
         commandList_->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

         const CD3DX12_RESOURCE_BARRIER toPresent =
            CD3DX12_RESOURCE_BARRIER::Transition(
               backBuffer,
               D3D12_RESOURCE_STATE_COPY_SOURCE,
               D3D12_RESOURCE_STATE_PRESENT);
         commandList_->ResourceBarrier(1, &toPresent);
      } else {
         Logger::Error(
            "Screenshot readback resource creation failed: "
            + screenshotPath->generic_string());
         screenshotPath.reset();
      }
   }

   if (!screenshotReadback) {
      // 通常経路でもスクリーンショット経路と同じPRESENT状態へ収束させる。
      const CD3DX12_RESOURCE_BARRIER toPresent =
         CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);
      commandList_->ResourceBarrier(1, &toPresent);
   }

   // コマンドリストの内容を確定させる。全てのコマンドをつんでからCloseすること
   result = commandList_->Close();
   assert(SUCCEEDED(result));

   // GPUにコマンドリストの実行を行わせる
   ID3D12CommandList* commandLists[] = { commandList_.Get() };
   commandQueue_->ExecuteCommandLists(1, commandLists);

   // GPUとOSに画面の交換を行うように通知する
   swapChain_->Present(1, 0);

#ifdef _DEBUG
   if (FAILED(result)) {
	  ComPtr<ID3D12DeviceRemovedExtendedData> dred;

	  result = device_->QueryInterface(IID_PPV_ARGS(&dred));
	  assert(SUCCEEDED(result));

	  D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT autoBreadcrumbsOutput{};
	  result = dred->GetAutoBreadcrumbsOutput(&autoBreadcrumbsOutput);
	  assert(SUCCEEDED(result));
   }
#endif

   // この実装はフレームごとにGPU完了を待つ。これにより単一のアロケータを安全に再利用でき、
   // スクリーンショット用リードバックも直後にCPUから参照できる。
   fenceValue_++;
   // GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
   commandQueue_->Signal(fence_.Get(), fenceValue_);

   // GetCompletedValueの初期値はFence作成時に渡した初期値
   if (fence_->GetCompletedValue() != fenceValue_) {
	  HANDLE event = CreateEvent(nullptr, false, false, nullptr);
	  fence_->SetEventOnCompletion(fenceValue_, event);
	  WaitForSingleObject(event, INFINITE);
	  CloseHandle(event);
	}

   if (screenshotPath && screenshotReadback) {
      // フェンス完了後なのでGPU書き込みとの競合はない。RowPitchはアライン済みの値を保持し、
      // WIC側へ実際のテクスチャ幅と併せて渡す。
      void* mappedData = nullptr;
      const D3D12_RANGE readRange = { 0, static_cast<SIZE_T>(screenshotBufferSize) };
      result = screenshotReadback->Map(0, &readRange, &mappedData);
      if (SUCCEEDED(result) && mappedData) {
         DirectX::Image image{};
         image.width = static_cast<size_t>(screenshotSourceDesc.Width);
         image.height = static_cast<size_t>(screenshotSourceDesc.Height);
         image.format = screenshotSourceDesc.Format;
         image.rowPitch = screenshotFootprint.Footprint.RowPitch;
         image.slicePitch = image.rowPitch * image.height;
         image.pixels =
            static_cast<uint8_t*>(mappedData) + screenshotFootprint.Offset;

         std::error_code directoryError;
         std::filesystem::create_directories(
            screenshotPath->parent_path(),
            directoryError);
         const HRESULT saveResult = directoryError
            ? HRESULT_FROM_WIN32(directoryError.value())
            : DirectX::SaveToWICFile(
               image,
               DirectX::WIC_FLAGS_FORCE_SRGB,
               GUID_ContainerFormatPng,
               screenshotPath->c_str());
         if (SUCCEEDED(saveResult)) {
            Logger::Info("Screenshot saved: " + screenshotPath->generic_string());
         } else {
            Logger::Error("Screenshot save failed: " + screenshotPath->generic_string());
         }

         const D3D12_RANGE writtenRange = { 0, 0 };
         screenshotReadback->Unmap(0, &writtenRange);
      } else {
         Logger::Error("Screenshot readback map failed: " + screenshotPath->generic_string());
      }
   }

   UpdateFixFPS();

   // 次のフレーム用のコマンドリストを準備
   result = commandAllocator_->Reset();
   assert(SUCCEEDED(result));
   result = commandList_->Reset(commandAllocator_.Get(), nullptr);
   assert(SUCCEEDED(result));
}

void GraphicsDevice::Finalize() {
   if (window_ && window_->IsFullscreen()) {
	  window_->SetFullscreen(false);
   }

   if (commandQueue_ && fence_) {
      // COMリソースを解放する前に、キューへ投入済みの全参照が切れるまで待つ。
      // ここを省くと終了時だけGPUが解放済みバックバッファを参照し得る。
	  fenceValue_++;
	  commandQueue_->Signal(fence_.Get(), fenceValue_);
	  if (fence_->GetCompletedValue() < fenceValue_) {
		 HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		 if (eventHandle) {
			fence_->SetEventOnCompletion(fenceValue_, eventHandle);
			WaitForSingleObject(eventHandle, INFINITE);
			CloseHandle(eventHandle);
		 }
	  }
   }

   for (auto& backBuffer : backBuffers_) {
	  backBuffer.Reset();
   }
   backBuffers_.clear();

   depthBuffer_.Reset();
   rtvHeap_.Reset();
   dsvHeap_.Reset();
   srvHeap_.Reset();
   swapChain_.Reset();
   commandList_.Reset();
   commandAllocator_.Reset();
   commandQueue_.Reset();
   fence_.Reset();
   device_.Reset();
   dxgiFactory_.Reset();
   window_ = nullptr;
}

void GraphicsDevice::EnableDebugLayer() {
#ifdef _DEBUG
   Microsoft::WRL::ComPtr<ID3D12Debug1> debugController;
   if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf())))) {
	  // デバッグレイヤーを有効化する
	  debugController->EnableDebugLayer();
	  // さらにGPU側でもチェックを行うようにする
	  debugController->SetEnableGPUBasedValidation(TRUE);
   }
#endif
}

void GraphicsDevice::InitializeDXGIDevice([[maybe_unused]] bool enableDebugLayer) {
#ifdef _DEBUG
   if (enableDebugLayer) {
	  EnableDebugLayer();  // DXGI Factory を作成する前に実行
   }
#endif

   HRESULT result = CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory_.GetAddressOf()));
   assert(SUCCEEDED(result));

   Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter;
   // 列挙順を高性能GPU優先とし、WARP等のソフトウェアアダプターを除外する。
   for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(
	  i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(adapter.GetAddressOf())) != DXGI_ERROR_NOT_FOUND; ++i) {

	  DXGI_ADAPTER_DESC3 desc;
	  adapter->GetDesc3(&desc);
	  if (!(desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
		 Logger::Info(std::format(L"Use Adapter: {}", desc.Description));
		 break;
	  }
   }
   assert(adapter != nullptr);

   // 機能レベルとログ出力用の文字列
   D3D_FEATURE_LEVEL featureLevels[] = {
	   D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
   };
   const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };

   // 同じ物理アダプター上で利用可能な最高機能レベルを選び、下位GPUにもフォールバックする。
   for (size_t i = 0; i < _countof(featureLevels); ++i) {
	  if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), featureLevels[i], IID_PPV_ARGS(device_.GetAddressOf())))) {
		 Logger::Info(std::format("FeatureLevel : {}", featureLevelStrings[i]));
		 break;
	  }
   }

   if (!device_) {
	  throw std::runtime_error("Failed to create D3D12 Device.");
   }

   Logger::Info("Complete create D3D12Device!!!");

#ifdef _DEBUG
   Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
   if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(infoQueue.GetAddressOf())))) {
	  // ヤバイエラー時に止まる
	  infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
	  // エラー時に止まる
	  infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
	  // 警告を止める
	  infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

	  // 抑制するメッセージのID
	  D3D12_MESSAGE_ID denyIds[] = {
		 // Windows11でのDXGIデバッグレイヤーの相互作用バグによるエラーメッセージ
		 // https://stackoverflow.com/questions/69805245/directx-12-application-is-windows-11
		 D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
	  };
	  // 抑制するレベル
	  D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
	  D3D12_INFO_QUEUE_FILTER filter{};
	  filter.DenyList.NumIDs = _countof(denyIds);
	  filter.DenyList.pIDList = denyIds;
	  filter.DenyList.NumSeverities = _countof(severities);
	  filter.DenyList.pSeverityList = severities;
	  // 指定したメッセージの表示を抑制する
	  infoQueue->PushStorageFilter(&filter);
   }
#endif

   descriptorSizeCBVSRVUAV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
   descriptorSizeRTV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
   descriptorSizeDSV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

void GraphicsDevice::InitializeCommand() {
   HRESULT result = S_FALSE;

   // コマンドアロケータを生成
   result = device_->CreateCommandAllocator(
	  D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator_.GetAddressOf()));
   assert(SUCCEEDED(result));

   // コマンドリストを生成
   result = device_->CreateCommandList(
	  0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr,
	  IID_PPV_ARGS(commandList_.GetAddressOf()));
   assert(SUCCEEDED(result));

   // コマンドキューを生成
   D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
   result = device_->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(commandQueue_.GetAddressOf()));
   assert(SUCCEEDED(result));
}

void GraphicsDevice::CreateSwapChain() {
   HRESULT result = S_FALSE;

   // スワップチェーンを生成する
   DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
   swapChainDesc.Width = backBufferWidth_;
   swapChainDesc.Height = backBufferHeight_;
   swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式
   swapChainDesc.SampleDesc.Count = 1; // マルチサンプルしない
   swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとして利用する
   swapChainDesc.BufferCount = 2; // ダブルバッファ
   swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // モニタにうつしたら、中身を破棄
   // コマンドキュー、ウィンドウハンドル、設定を渡して生成する
   result = dxgiFactory_->CreateSwapChainForHwnd(commandQueue_.Get(), window_->GetHwnd(), &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf()));
   assert(SUCCEEDED(result));

   result = dxgiFactory_->MakeWindowAssociation(window_->GetHwnd(), DXGI_MWA_NO_ALT_ENTER);
   assert(SUCCEEDED(result));
}

void GraphicsDevice::CreateRenderTargetViews() {
   HRESULT result = S_FALSE;

   DXGI_SWAP_CHAIN_DESC swcDesc = {};
   result = swapChain_->GetDesc(&swcDesc);
   assert(SUCCEEDED(result));

   // RTVヒープはオフスクリーンRTが2番以降のCPUハンドルを保持している。
   // リサイズ時もヒープ自体を再生成せず、既存ハンドルの有効性を保つ。
   if (!rtvHeap_) {
	  D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	  heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビュー
	  heapDesc.NumDescriptors = rtvCount_;
	  result = device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(rtvHeap_.ReleaseAndGetAddressOf()));
	  assert(SUCCEEDED(result));
   }

   backBuffers_.resize(swcDesc.BufferCount);
   for (int i = 0; i < backBuffers_.size(); i++) {
	  // スワップチェーンからバッファを取得
    result = swapChain_->GetBuffer(i, IID_PPV_ARGS(backBuffers_[i].ReleaseAndGetAddressOf()));
	  assert(SUCCEEDED(result));

	  // ディスクリプタヒープのハンドルを取得
	  CD3DX12_CPU_DESCRIPTOR_HANDLE handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
		 rtvHeap_->GetCPUDescriptorHandleForHeapStart(), i,
		 device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
	  // レンダーターゲットビューの設定
	  D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{};
	  // シェーダーの計算結果をSRGBに変換して書き込む
	  renderTargetViewDesc.Format = rtvFormat_;
	  renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	  // レンダーターゲットビューの生成
	  device_->CreateRenderTargetView(backBuffers_[i].Get(), &renderTargetViewDesc, handle);
   }
}

void GraphicsDevice::CreateDepthStencilViews() {
   HRESULT result = S_FALSE;

   // ヒーププロパティ
   CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

   // 同一リソースをDSVとSRVの両方から見るため、実体はTYPELESSで作り、
   // 各ビュー側で深度書き込み用／深度読み取り用の型を確定する。
   CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
	  DXGI_FORMAT_R24G8_TYPELESS,
	  backBufferWidth_, backBufferHeight_,
	  1, 1, 1, 0,
	  D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
   );

   CD3DX12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, 0);

   // リソースの生成
   result = device_->CreateCommittedResource(
	  &heapProperties,
	  D3D12_HEAP_FLAG_NONE,
	  &resourceDesc,
	  D3D12_RESOURCE_STATE_DEPTH_WRITE,
	  &clearValue,
   IID_PPV_ARGS(depthBuffer_.ReleaseAndGetAddressOf())
   );

   assert(SUCCEEDED(result));
   depthBufferState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;

   // DSV用のヒープでディスクリプタの数は1つ
   D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
   dsvHeapDesc.NumDescriptors = 1;
   dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
   result = device_->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(dsvHeap_.ReleaseAndGetAddressOf()));
   assert(SUCCEEDED(result));

   // DSVの設定
   D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
   dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
   dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

   device_->CreateDepthStencilView(
	  depthBuffer_.Get(), &dsvDesc, dsvHeap_->GetCPUDescriptorHandleForHeapStart()
   );

   if (srvHeap_) {
      // リサイズ後もルートテーブルが保持するGPUハンドルを変えないよう、
      // 初回に確保したディスクリプタ位置へ新しい深度SRVを上書きする。
	  if (depthSrvIndex_ == static_cast<UINT>(-1)) {
		 depthSrvIndex_ = GetNextSrvIndex();
		 depthSrvHandleCPU_ = CD3DX12_CPU_DESCRIPTOR_HANDLE(
			srvHeap_->GetCPUDescriptorHandleForHeapStart(),
			depthSrvIndex_,
			descriptorSizeCBVSRVUAV);
		 depthSrvHandleGPU_ = CD3DX12_GPU_DESCRIPTOR_HANDLE(
			srvHeap_->GetGPUDescriptorHandleForHeapStart(),
			depthSrvIndex_,
			descriptorSizeCBVSRVUAV);
		 IncrementSrvIndex();
	  }

	  D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc{};
	  depthSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	  depthSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	  depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	  depthSrvDesc.Texture2D.MipLevels = 1;

	  device_->CreateShaderResourceView(depthBuffer_.Get(), &depthSrvDesc, depthSrvHandleCPU_);
   }
}

void GraphicsDevice::CreateSRVHeap() {
   srvHeap_ = CreateDescriptorHeap(
      device_.Get(),
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
      kShaderVisibleDescriptorCount,
      true);
}

void GraphicsDevice::CreateFence() {
   HRESULT result = S_FALSE;
   result = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf()));
   assert(SUCCEEDED(result));
}

void GraphicsDevice::TransitionDepthStencilToShaderResource() {
   if (!depthBuffer_ || depthBufferState_ == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
	  return;
   }

   // 呼び出し側が直前の用途を意識せずに済むよう、追跡中の実状態をbeforeに使う。
   CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
	  depthBuffer_.Get(),
	  depthBufferState_,
	  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
   commandList_->ResourceBarrier(1, &barrier);
   depthBufferState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void GraphicsDevice::TransitionDepthStencilToWrite() {
   if (!depthBuffer_ || depthBufferState_ == D3D12_RESOURCE_STATE_DEPTH_WRITE) {
	  return;
   }

   CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
	  depthBuffer_.Get(),
	  depthBufferState_,
	  D3D12_RESOURCE_STATE_DEPTH_WRITE);
   commandList_->ResourceBarrier(1, &barrier);
   depthBufferState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
}

void GraphicsDevice::TransitionDepthStencilToCopySource() {
   if (!depthBuffer_ || depthBufferState_ == D3D12_RESOURCE_STATE_COPY_SOURCE) {
	  return;
   }

   CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
	  depthBuffer_.Get(),
	  depthBufferState_,
	  D3D12_RESOURCE_STATE_COPY_SOURCE);
   commandList_->ResourceBarrier(1, &barrier);
   depthBufferState_ = D3D12_RESOURCE_STATE_COPY_SOURCE;
}

void GraphicsDevice::InitializeFixFPS() {
   reference_ = std::chrono::high_resolution_clock::now();
}

void GraphicsDevice::UpdateFixFPS() {
   // 現在時間を取得
   std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
   // 前回記録からの経過時間を取得
   std::chrono::microseconds elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

   // 目標時刻から十分遠い場合だけ待機ループへ入る。65fps相当を入口にすることで、
   // 既に処理が重いフレームへ追加のsleepを入れず、60fps到達までの微小待機だけを行う。
   if (elapsed < kSleepCheckTime) {
	  while (std::chrono::steady_clock::now() - reference_ < kTargetFrameTime) {
		 std::this_thread::sleep_for(kSpinSleepInterval);
	  }
   }

   reference_ = std::chrono::steady_clock::now();
}

void GraphicsDevice::ExecuteCommandListAndWait() {
   // リサイズ等で参照中のGPUリソースを作り直す前に、現在までのコマンドを明示的に
   // フラッシュして完了を待つ。通常のPresentを伴わない同期経路として使用する。
   HRESULT hr = commandList_->Close();
   assert(SUCCEEDED(hr));

   // 実行
   ID3D12CommandList* commandLists[] = { commandList_.Get() };
   commandQueue_->ExecuteCommandLists(_countof(commandLists), commandLists);

   // フェンス値をインクリメント
   fenceValue_++;
   commandQueue_->Signal(fence_.Get(), fenceValue_);

   // 完了を待つ
   if (fence_->GetCompletedValue() < fenceValue_) {
	  HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	  assert(eventHandle != nullptr);

	  fence_->SetEventOnCompletion(fenceValue_, eventHandle);
	  WaitForSingleObject(eventHandle, INFINITE);
	  CloseHandle(eventHandle);
   }

   // コマンドアロケータとリストをリセット（次のコマンドのため）
   commandAllocator_->Reset();
   commandList_->Reset(commandAllocator_.Get(), nullptr);
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GraphicsDevice::CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible)
{
   ComPtr<ID3D12DescriptorHeap> descriptorHeap;
   D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
   descriptorHeapDesc.Type = heapType;
   descriptorHeapDesc.NumDescriptors = numDescriptors;
   descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
   HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(descriptorHeap.GetAddressOf()));
   assert(SUCCEEDED(hr));
   return descriptorHeap;
}

void GraphicsDevice::ToggleFullscreen() {
   if (!window_) {
	  return;
   }

   window_->ToggleFullscreen();
   SyncBackBufferSizeToWindow();
}

void GraphicsDevice::SyncBackBufferSizeToWindow() {
   if (!window_ || !window_->GetHwnd()) {
	  return;
   }

   RECT clientRect{};
   if (!::GetClientRect(window_->GetHwnd(), &clientRect)) {
	  return;
   }

   const LONG width = clientRect.right - clientRect.left;
   const LONG height = clientRect.bottom - clientRect.top;
   if (width <= 0 || height <= 0) {
	  return;
   }

   ResizeSwapChainResources(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
}

void GraphicsDevice::ResizeSwapChainResources(uint32_t width, uint32_t height) {
   if (!swapChain_) {
	  return;
   }

   if (backBufferWidth_ == static_cast<int32_t>(width) && backBufferHeight_ == static_cast<int32_t>(height)) {
	  return;
   }

   // ResizeBuffersはキューから参照中のバックバッファに対して実行できないため、
   // 先にコマンドを完了させてから全参照を解放する。
   ExecuteCommandListAndWait();

   for (auto& backBuffer : backBuffers_) {
	  backBuffer.Reset();
   }
   depthBuffer_.Reset();

   HRESULT hr = swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
   assert(SUCCEEDED(hr));

   backBufferWidth_ = static_cast<int32_t>(width);
   backBufferHeight_ = static_cast<int32_t>(height);

   CreateRenderTargetViews();
   CreateDepthStencilViews();
}

UINT GraphicsDevice::GetNextSrvIndex() const {
   // 解放済みスロットを先に再利用し、長時間のアセット差し替えでヒープ末尾が
   // 一方向に消費され続けることを防ぐ。確保確定はIncrementSrvIndexで行う。
   if (!freeSrvIndices_.empty()) {
      return freeSrvIndices_.front();
   }
   return nextSrvIndex_;
}

void GraphicsDevice::IncrementSrvIndex() {
   if (!freeSrvIndices_.empty()) {
      freeSrvIndices_.pop();
   } else {
      ++nextSrvIndex_;
   }
}

void GraphicsDevice::ReleaseSrvIndex(UINT index) {
   freeSrvIndices_.push(index);
}
}
