#include "pch.h"
#include "GraphicsDevice.h"
#include "ImGuiManager.h"
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

namespace {
Logger& log_ = Logger::GetInstance();
}

namespace GameEngine {
void GraphicsDevice::Initialize(Window* window, int32_t backBufferWidth, int32_t backBufferHeight, bool enableDebugLayer) {
   window_ = window;
   backBufferWidth_ = backBufferWidth;
   backBufferHeight_ = backBufferHeight;

   InitializeDXGIDevice(enableDebugLayer);

   InitializeCommand();

   InitializeFixFPS();

   CreateSwapChain();

   CreateRenderTargetViews();

   CreateDepthStencilViews();

   CreateSRVHeap();

   CreateFence();
}

void GraphicsDevice::PreDraw() {
   // これから書き込むバックバッファのインデックスを取得
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

   // 描画先のRTVを設定する
   commandList_->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

   // 指定した深度で画面全体をクリアする
   commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

   // 指定した色で画面全体をクリアする
   float clearColor[] = { 0.1f,0.25f,0.5f,1.0f }; // 青っぽい色。RGBAの順
   commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

   // ビューポートの設定
   CD3DX12_VIEWPORT viewport =
	  CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(backBufferWidth_), static_cast<float>(backBufferHeight_));
   commandList_->RSSetViewports(1, &viewport);
   // シザリング矩形の設定
   CD3DX12_RECT scissorRect = CD3DX12_RECT(0, 0, backBufferWidth_, backBufferHeight_);
   commandList_->RSSetScissorRects(1, &scissorRect);

   ComPtr<ID3D12DescriptorHeap> descriptorHeaps[] = { srvHeap_ };
   commandList_->SetDescriptorHeaps(1, descriptorHeaps->GetAddressOf());
}

void GraphicsDevice::PostDraw() {
   HRESULT result = S_FALSE;

   // リソースバリアを変更（描画対象→表示状態）
   UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
   CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
	  backBuffers_[backBufferIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
	  D3D12_RESOURCE_STATE_PRESENT);
   commandList_->ResourceBarrier(1, &barrier);

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

   // Fenceの値を更新
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

   UpdateFixFPS();

   // 次のフレーム用のコマンドリストを準備
   result = commandAllocator_->Reset();
   assert(SUCCEEDED(result));
   result = commandList_->Reset(commandAllocator_.Get(), nullptr);
   assert(SUCCEEDED(result));
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
   for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(
	  i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(adapter.GetAddressOf())) != DXGI_ERROR_NOT_FOUND; ++i) {

	  DXGI_ADAPTER_DESC3 desc;
	  adapter->GetDesc3(&desc);
	  if (!(desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
		 log_.Log(std::format(L"Use Adapter: {}", desc.Description));
		 break;
	  }
   }
   assert(adapter != nullptr);

   // 機能レベルとログ出力用の文字列
   D3D_FEATURE_LEVEL featureLevels[] = {
	   D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
   };
   const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };

   // 高い順に生成できるか試していく
   for (size_t i = 0; i < _countof(featureLevels); ++i) {
	  if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), featureLevels[i], IID_PPV_ARGS(device_.GetAddressOf())))) {
		 log_.Log(std::format("FeatureLevel : {}", featureLevelStrings[i]));
		 break;
	  }
   }

   if (!device_) {
	  throw std::runtime_error("Failed to create D3D12 Device.");
   }

   log_.Log("Complete create D3D12Device!!!");

#ifdef _DEBUG
   Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
   if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(infoQueue.GetAddressOf())))) {
	  // ヤバイエラー時に止まる
	  infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
	  // エラー時に止まる
	  infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
	  // 警告時に止まる
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
}

void GraphicsDevice::CreateRenderTargetViews() {
   HRESULT result = S_FALSE;

   DXGI_SWAP_CHAIN_DESC swcDesc = {};
   result = swapChain_->GetDesc(&swcDesc);
   assert(SUCCEEDED(result));

   // 各種設定をしてディスクリプタヒープを生成
   D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
   heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビュー
   heapDesc.NumDescriptors = rtvCount_;
   result = device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(rtvHeap_.GetAddressOf()));
   assert(SUCCEEDED(result));

   backBuffers_.resize(swcDesc.BufferCount);
   for (int i = 0; i < backBuffers_.size(); i++) {
	  // スワップチェーンからバッファを取得
	  result = swapChain_->GetBuffer(i, IID_PPV_ARGS(backBuffers_[i].GetAddressOf()));
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

   // リソース設定
   CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
	  DXGI_FORMAT_D24_UNORM_S8_UINT,
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
	  IID_PPV_ARGS(depthBuffer_.GetAddressOf())
   );

   assert(SUCCEEDED(result));

   // DSV用のヒープでディスクリプタの数は1つ
   D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
   dsvHeapDesc.NumDescriptors = 1;
   dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
   result = device_->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(dsvHeap_.GetAddressOf()));
   assert(SUCCEEDED(result));

   // DSVの設定
   D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
   dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
   dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

   device_->CreateDepthStencilView(
	  depthBuffer_.Get(), &dsvDesc, dsvHeap_->GetCPUDescriptorHandleForHeapStart()
   );
}

void GraphicsDevice::CreateSRVHeap() {
   srvHeap_ = CreateDescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4096, true);
}

void GraphicsDevice::CreateFence() {
   HRESULT result = S_FALSE;
   result = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf()));
   assert(SUCCEEDED(result));
}

void GraphicsDevice::InitializeFixFPS() {
   reference_ = std::chrono::high_resolution_clock::now();
}

void GraphicsDevice::UpdateFixFPS() {
   const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f));
   const std::chrono::microseconds kMinCheckTime(uint64_t(1000000.0f / 65.0f));

   // 現在時間を取得
   std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
   // 前回記録からの経過時間を取得
   std::chrono::microseconds elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

   // 1/60秒（よりわずかに短い時間）経っていない場合
   if (elapsed < kMinCheckTime) {
	  while (std::chrono::steady_clock::now() - reference_ < kMinTime) {
		 std::this_thread::sleep_for(std::chrono::microseconds(1));
	  }
   }

   reference_ = std::chrono::steady_clock::now();
}

void GraphicsDevice::ExecuteCommandListAndWait() {
   // コマンドリストを閉じる
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
}
