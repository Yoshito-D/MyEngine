#include "GameEngine/pch.h"
#include <cstdint>
#include <limits>
#include <stdexcept>
#include "GameEngine/Graphics/Device/GraphicsDevice.h"
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

namespace GameEngine {
namespace {
constexpr UINT kShaderVisibleDescriptorCount = 4096;
constexpr float kBackBufferClearColor[] = {0.1f, 0.25f, 0.5f, 1.0f};
constexpr int64_t kMicrosecondsPerSecond = 1'000'000;
constexpr int64_t kTargetFramesPerSecond = 60;
constexpr int64_t kSleepCheckFramesPerSecond = 65;
constexpr std::chrono::microseconds kTargetFrameTime(kMicrosecondsPerSecond / kTargetFramesPerSecond);
constexpr std::chrono::microseconds kSleepCheckTime(kMicrosecondsPerSecond / kSleepCheckFramesPerSecond);
constexpr std::chrono::microseconds kSpinSleepInterval(1);

void CheckGraphicsResult(HRESULT result, const char* operation, ID3D12Device* device = nullptr) {
   if (SUCCEEDED(result)) return;

   std::string message = std::format("{} failed: HRESULT=0x{:08X}", operation, static_cast<UINT>(result));

   if (device) {
      const HRESULT removedReason = device->GetDeviceRemovedReason();

      if (FAILED(removedReason)) {
         message += std::format(", device removal reason=0x{:08X}", static_cast<UINT>(removedReason));
      }
   }

   Logger::Fail(message);
}
}

void GraphicsDevice::Initialize(
   Window* window, int32_t backBufferWidth, int32_t backBufferHeight, bool enableDebugLayer) {
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

void GraphicsDevice::InitializeOffscreen(ID3D12Device* device) {
   if (!device || device_) {
      throw std::invalid_argument("InitializeOffscreen requires a device and an uninitialized GraphicsDevice");
   }

   device_ = device;
   descriptorSizeCBVSRVUAV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
   descriptorSizeRTV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
   descriptorSizeDSV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
   InitializeCommand();
   CreateSRVHeap();
   CreateFence();
}

void GraphicsDevice::PreDraw() {
   // Present済みのバッファへ直接書き込めないため、このフレームが所有する
   // バックバッファだけをRENDER_TARGETへ遷移させる。
   UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

   // リソースバリアを変更（表示状態→描画対象）
   CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      backBuffers_[backBufferIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
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
   ComPtr<ID3D12DescriptorHeap> descriptorHeaps[] = {srvHeap_};
   commandList_->SetDescriptorHeaps(1, descriptorHeaps->GetAddressOf());
}

void GraphicsDevice::PostDraw() {
   const UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
   ID3D12Resource* backBuffer = backBuffers_[backBufferIndex].Get();
   const CD3DX12_RESOURCE_BARRIER toPresent = CD3DX12_RESOURCE_BARRIER::Transition(
      backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
   commandList_->ResourceBarrier(1, &toPresent);

   SubmitCommandList();
   CheckGraphicsResult(swapChain_->Present(1, 0), "IDXGISwapChain::Present", device_.Get());

   // 単一のアロケータを安全に再利用するため、フレームごとにGPU完了を待つ。
   WaitForIdle();
   UpdateFixFPS();
   ResetCommandList();
}

void GraphicsDevice::WaitForIdle() {
   if (!commandsInFlight_) return;

   ++fenceValue_;
   CheckGraphicsResult(commandQueue_->Signal(fence_.Get(), fenceValue_), "ID3D12CommandQueue::Signal", device_.Get());

   const auto checkCompletion = [this]() {
      const UINT64 completed = fence_->GetCompletedValue();

      // デバイス消失時の戻り値をフェンス到達として誤認しない。
      if (completed == std::numeric_limits<UINT64>::max()) {
         CheckGraphicsResult(DXGI_ERROR_DEVICE_REMOVED, "ID3D12Fence::GetCompletedValue", device_.Get());
      }

      return completed >= fenceValue_;
   };

   if (!checkCompletion()) {
      CheckGraphicsResult(fence_->SetEventOnCompletion(fenceValue_, fenceEvent_.Get()),
         "ID3D12Fence::SetEventOnCompletion", device_.Get());
      const DWORD waitResult = WaitForSingleObject(fenceEvent_.Get(), INFINITE);

      if (waitResult == WAIT_FAILED) {
         Logger::Fail(std::format("WaitForSingleObject failed for GPU fence: Win32={}", GetLastError()));
      }

      if (waitResult != WAIT_OBJECT_0 || !checkCompletion()) {
         Logger::Fail(std::format("GPU fence {} did not complete: wait result={}", fenceValue_, waitResult));
      }
   }

   commandsInFlight_ = false;
}

void GraphicsDevice::Finalize() {
   if (window_ && window_->IsFullscreen()) {
      window_->SetFullscreen(false);
   }

   try {
      WaitForIdle();
   } catch (const std::exception&) {
      // 同期の失敗は発生箇所で記録済み。終了時はデバイスが失われていても所有資源を解放する。
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
   fenceEvent_.Close();
   commandsInFlight_ = false;
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
      EnableDebugLayer(); // DXGI Factory を作成する前に実行
   }
#endif

   const HRESULT factoryResult = CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory_.GetAddressOf()));

   if (FAILED(factoryResult)) {
      Logger::Fail(std::format("Cannot create DXGI factory: HRESULT=0x{:08X}", static_cast<UINT>(factoryResult)));
   }

   Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter;

   // 列挙順を高性能GPU優先とし、WARP等のソフトウェアアダプターを除外する。
   for (UINT index = 0; !adapter; ++index) {
      Microsoft::WRL::ComPtr<IDXGIAdapter4> candidate;
      const HRESULT result = dxgiFactory_->EnumAdapterByGpuPreference(
         index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(candidate.GetAddressOf()));

      if (result == DXGI_ERROR_NOT_FOUND) {
         Logger::Fail("No hardware graphics adapter found");
      }

      if (FAILED(result)) {
         Logger::Fail(std::format("Cannot enumerate graphics adapters: HRESULT=0x{:08X}", static_cast<UINT>(result)));
      }

      DXGI_ADAPTER_DESC3 desc{};
      const HRESULT descResult = candidate->GetDesc3(&desc);

      if (FAILED(descResult)) {
         Logger::Fail(std::format("Cannot read graphics adapter info: HRESULT=0x{:08X}", static_cast<UINT>(descResult)));
      }

      if (!(desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
         Logger::Info(std::format(L"Use Adapter: {}", desc.Description));
         adapter = std::move(candidate);
      }
   }

   // D3D12CreateDeviceには使用できる最小機能レベルを指定する。
   constexpr D3D_FEATURE_LEVEL kMinimumFeatureLevel = D3D_FEATURE_LEVEL_12_0;
   const HRESULT deviceResult =
      D3D12CreateDevice(adapter.Get(), kMinimumFeatureLevel, IID_PPV_ARGS(device_.GetAddressOf()));

   if (FAILED(deviceResult)) {
      Logger::Fail(std::format("Cannot create D3D12 device (minimum feature level 12.0): HRESULT=0x{:08X}",
         static_cast<UINT>(deviceResult)));
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
      D3D12_MESSAGE_ID denyIds[] = {// Windows11でのDXGIデバッグレイヤーの相互作用バグによるエラーメッセージ

         // https://stackoverflow.com/questions/69805245/directx-12-application-is-windows-11
         D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE};

      // 抑制するレベル
      D3D12_MESSAGE_SEVERITY severities[] = {D3D12_MESSAGE_SEVERITY_INFO};
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
   CheckGraphicsResult(
      device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator_.GetAddressOf())),
      "ID3D12Device::CreateCommandAllocator", device_.Get());
   CheckGraphicsResult(device_->CreateCommandList(
                          0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr,
                          IID_PPV_ARGS(commandList_.GetAddressOf())),
      "ID3D12Device::CreateCommandList", device_.Get());

   const D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
   CheckGraphicsResult(device_->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(commandQueue_.GetAddressOf())),
      "ID3D12Device::CreateCommandQueue", device_.Get());
}

void GraphicsDevice::CreateSwapChain() {
   // スワップチェーンを生成する
   DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
   swapChainDesc.Width = backBufferWidth_;
   swapChainDesc.Height = backBufferHeight_;
   swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;           // 色の形式
   swapChainDesc.SampleDesc.Count = 1;                          // マルチサンプルしない
   swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとして利用する
   swapChainDesc.BufferCount = 2;                               // ダブルバッファ
   swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;    // モニタにうつしたら、中身を破棄

   // コマンドキュー、ウィンドウハンドル、設定を渡して生成する
   ComPtr<IDXGISwapChain1> swapChain;
   CheckGraphicsResult(dxgiFactory_->CreateSwapChainForHwnd(commandQueue_.Get(), window_->GetHwnd(), &swapChainDesc,
                          nullptr, nullptr, swapChain.GetAddressOf()),
      "IDXGIFactory::CreateSwapChainForHwnd", device_.Get());
   CheckGraphicsResult(swapChain.As(&swapChain_), "IDXGISwapChain4::QueryInterface", device_.Get());
   CheckGraphicsResult(dxgiFactory_->MakeWindowAssociation(window_->GetHwnd(), DXGI_MWA_NO_ALT_ENTER),
      "IDXGIFactory::MakeWindowAssociation", device_.Get());
}

void GraphicsDevice::CreateRenderTargetViews() {
   DXGI_SWAP_CHAIN_DESC swcDesc{};
   CheckGraphicsResult(swapChain_->GetDesc(&swcDesc), "IDXGISwapChain::GetDesc", device_.Get());

   // RTVヒープはオフスクリーンRTが2番以降のCPUハンドルを保持している。
   // リサイズ時もヒープ自体を再生成せず、既存ハンドルの有効性を保つ。
   if (!rtvHeap_) {
      D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
      heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビュー
      heapDesc.NumDescriptors = rtvCount_;
      CheckGraphicsResult(device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(rtvHeap_.GetAddressOf())),
         "ID3D12Device::CreateDescriptorHeap (RTV)", device_.Get());
   }

   backBuffers_.resize(swcDesc.BufferCount);

   for (std::int32_t i = 0; i < backBuffers_.size(); i++) {
      // スワップチェーンからバッファを取得
      CheckGraphicsResult(swapChain_->GetBuffer(i, IID_PPV_ARGS(backBuffers_[i].ReleaseAndGetAddressOf())),
         "IDXGISwapChain::GetBuffer", device_.Get());

      // ディスクリプタヒープのハンドルを取得
      CD3DX12_CPU_DESCRIPTOR_HANDLE handle =
         CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeap_->GetCPUDescriptorHandleForHeapStart(), i,
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
   // ヒーププロパティ
   CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

   // 同一リソースをDSVとSRVの両方から見るため、実体はTYPELESSで作り、
   // 各ビュー側で深度書き込み用／深度読み取り用の型を確定する。
   CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R24G8_TYPELESS, backBufferWidth_,
      backBufferHeight_, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

   CD3DX12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, 0);

   // リソースの生成
   CheckGraphicsResult(
      device_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
         D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(depthBuffer_.ReleaseAndGetAddressOf())),
      "ID3D12Device::CreateCommittedResource (depth buffer)", device_.Get());
   depthBufferState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;

   // DSV用のヒープでディスクリプタの数は1つ
   D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
   dsvHeapDesc.NumDescriptors = 1;
   dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
   CheckGraphicsResult(device_->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(dsvHeap_.ReleaseAndGetAddressOf())),
      "ID3D12Device::CreateDescriptorHeap (DSV)", device_.Get());

   // DSVの設定
   D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
   dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
   dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

   device_->CreateDepthStencilView(depthBuffer_.Get(), &dsvDesc, dsvHeap_->GetCPUDescriptorHandleForHeapStart());

   if (srvHeap_) {
      // リサイズ後もルートテーブルが保持するGPUハンドルを変えないよう、
      // 初回に確保したディスクリプタ位置へ新しい深度SRVを上書きする。
      if (depthSrvIndex_ == static_cast<UINT>(-1)) {
         depthSrvIndex_ = GetNextSrvIndex();
         depthSrvHandleCPU_ = CD3DX12_CPU_DESCRIPTOR_HANDLE(
            srvHeap_->GetCPUDescriptorHandleForHeapStart(), depthSrvIndex_, descriptorSizeCBVSRVUAV);
         depthSrvHandleGPU_ = CD3DX12_GPU_DESCRIPTOR_HANDLE(
            srvHeap_->GetGPUDescriptorHandleForHeapStart(), depthSrvIndex_, descriptorSizeCBVSRVUAV);
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
   srvHeap_ =
      CreateDescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kShaderVisibleDescriptorCount, true);
}

void GraphicsDevice::CreateFence() {
   CheckGraphicsResult(device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf())),
      "ID3D12Device::CreateFence", device_.Get());
   const HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);

   if (!event) {
      Logger::Fail(std::format("CreateEventW failed for GPU fence: Win32={}", GetLastError()));
   }

   fenceEvent_.Attach(event);
}

void GraphicsDevice::TransitionDepthStencilToShaderResource() {
   if (!depthBuffer_ || depthBufferState_ == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) return;

   // 呼び出し側が直前の用途を意識せずに済むよう、追跡中の実状態をbeforeに使う。
   CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      depthBuffer_.Get(), depthBufferState_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
   commandList_->ResourceBarrier(1, &barrier);
   depthBufferState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void GraphicsDevice::TransitionDepthStencilToWrite() {
   if (!depthBuffer_ || depthBufferState_ == D3D12_RESOURCE_STATE_DEPTH_WRITE) return;

   CD3DX12_RESOURCE_BARRIER barrier =
      CD3DX12_RESOURCE_BARRIER::Transition(depthBuffer_.Get(), depthBufferState_, D3D12_RESOURCE_STATE_DEPTH_WRITE);
   commandList_->ResourceBarrier(1, &barrier);
   depthBufferState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
}

void GraphicsDevice::TransitionDepthStencilToCopySource() {
   if (!depthBuffer_ || depthBufferState_ == D3D12_RESOURCE_STATE_COPY_SOURCE) return;

   CD3DX12_RESOURCE_BARRIER barrier =
      CD3DX12_RESOURCE_BARRIER::Transition(depthBuffer_.Get(), depthBufferState_, D3D12_RESOURCE_STATE_COPY_SOURCE);
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
   SubmitCommandList();
   WaitForIdle();
   ResetCommandList();
}

void GraphicsDevice::SubmitCommandList() {
   CheckGraphicsResult(commandList_->Close(), "ID3D12GraphicsCommandList::Close", device_.Get());
   ID3D12CommandList* commandLists[] = {commandList_.Get()};
   commandQueue_->ExecuteCommandLists(static_cast<UINT>(std::size(commandLists)), commandLists);
   commandsInFlight_ = true;
}

void GraphicsDevice::ResetCommandList() {
   CheckGraphicsResult(commandAllocator_->Reset(), "ID3D12CommandAllocator::Reset", device_.Get());
   CheckGraphicsResult(commandList_->Reset(commandAllocator_.Get(), nullptr), "ID3D12GraphicsCommandList::Reset",
      device_.Get());
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GraphicsDevice::CreateDescriptorHeap(
   ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
   ComPtr<ID3D12DescriptorHeap> descriptorHeap;
   D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
   descriptorHeapDesc.Type = heapType;
   descriptorHeapDesc.NumDescriptors = numDescriptors;
   descriptorHeapDesc.Flags =
      shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
   CheckGraphicsResult(device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(descriptorHeap.GetAddressOf())),
      "ID3D12Device::CreateDescriptorHeap", device);
   return descriptorHeap;
}

void GraphicsDevice::ToggleFullscreen() {
   if (!window_) return;

   window_->ToggleFullscreen();
   SyncBackBufferSizeToWindow();
}

void GraphicsDevice::SyncBackBufferSizeToWindow() {
   if (!window_ || !window_->GetHwnd()) return;

   RECT clientRect{};

   if (!::GetClientRect(window_->GetHwnd(), &clientRect)) return;

   const LONG width = clientRect.right - clientRect.left;
   const LONG height = clientRect.bottom - clientRect.top;

   if (width <= 0 || height <= 0) return;

   ResizeSwapChainResources(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
}

void GraphicsDevice::ResizeSwapChainResources(uint32_t width, uint32_t height) {
   if (!swapChain_) return;

   if (backBufferWidth_ == static_cast<int32_t>(width) && backBufferHeight_ == static_cast<int32_t>(height)) return;

   // ResizeBuffersはキューから参照中のバックバッファに対して実行できないため、
   // 先にコマンドを完了させてから全参照を解放する。
   ExecuteCommandListAndWait();

   for (auto& backBuffer : backBuffers_) {
      backBuffer.Reset();
   }

   depthBuffer_.Reset();

   CheckGraphicsResult(swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0),
      "IDXGISwapChain::ResizeBuffers", device_.Get());

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

   if (nextSrvIndex_ >= kShaderVisibleDescriptorCount) {
      throw std::overflow_error("CBV/SRV/UAV descriptor heap is full");
   }

   return nextSrvIndex_;
}

void GraphicsDevice::IncrementSrvIndex() {
   (void)GetNextSrvIndex(); // Getを経由しない呼び出しでもヒープ容量を超えない。

   if (!freeSrvIndices_.empty()) {
      freeSrvIndices_.pop();
   } else {
      ++nextSrvIndex_;
   }
}

SrvDescriptorAllocation::~SrvDescriptorAllocation() {
   if (device_) {
      device_->ReleaseSrvIndex(index_);
   }
}

std::shared_ptr<SrvDescriptorAllocation> SrvDescriptorAllocation::Allocate(GraphicsDevice& device) {
   // 共有所有権の確保が失敗しても未確保のスロットを返却しないよう、確定後にdeviceを渡す。
   auto allocation = std::shared_ptr<SrvDescriptorAllocation>(new SrvDescriptorAllocation(device.GetNextSrvIndex()));
   device.IncrementSrvIndex();
   allocation->device_ = &device;
   return allocation;
}

std::shared_ptr<SrvDescriptorAllocation> GraphicsDevice::AllocateSrvDescriptor() {
   return SrvDescriptorAllocation::Allocate(*this);
}

void GraphicsDevice::ReleaseSrvIndex(UINT index) {
   freeSrvIndices_.push(index);
}
}
