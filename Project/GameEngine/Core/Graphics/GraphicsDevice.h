#pragma once
#include "Core/Window/Window.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <vector>
#include <memory>
#include <chrono>
#include <filesystem>
#include <optional>
#include <queue>

namespace GameEngine {
/// @brief グラフィックスデバイスクラス
class GraphicsDevice {
public:
   template<typename T>
   using ComPtr = Microsoft::WRL::ComPtr<T>;

   /// @brief デバイスなどの初期化
   /// @param window ウィンドウ 
   /// @param backBufferWidth ウィンドウの幅
   /// @param backBufferHeight ウィンドウの高さ 
   /// @param enableDebugLayer デバッグレイヤーの有無 
   void Initialize(Window* window, int32_t backBufferWidth = Window::kWindowWidth, int32_t backBufferHeight = Window::kWindowHeight, bool enableDebugLayer = true);

   /// @brief ループ開始時の処理
   void PreDraw();

   /// @brief ループ終了時の処理
   void PostDraw();

   /// @brief 次に完成するバックバッファをPNGへ保存する
   /// @param outputPath 保存先PNGパス
   void RequestScreenshot(const std::filesystem::path& outputPath);

   /// @brief 終了処理
   void Finalize();

   /// @brief デバイスを取得
   /// @return デバイス
   ID3D12Device* GetDevice() const { return device_.Get(); }

   /// @brief コマンドリストを取得
   /// @return コマンドリスト
   ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }

   /// @brief スワップチェーンを取得
   /// @return スワップチェーン
   IDXGISwapChain4* GetSwapChain() const { return swapChain_.Get(); }

   /// @brief SRVヒープを取得
   /// @return SRVヒープ
   ID3D12DescriptorHeap* GetSRVHeap() const { return srvHeap_.Get(); }

   /// @brief RTVヒープを取得
   /// @return RTVヒープ
   ID3D12DescriptorHeap* GetRTVHeap() const { return rtvHeap_.Get(); }

   /// @brief DSVヒープを取得
   /// @return DSVヒープ
   ID3D12DescriptorHeap* GetDSVHeap() const { return dsvHeap_.Get(); }

   /// @brief 深度バッファのSRVハンドル（GPU）を取得
   /// @return 深度バッファSRVハンドル
   D3D12_GPU_DESCRIPTOR_HANDLE GetDepthSRVHandleGPU() const { return depthSrvHandleGPU_; }

   /// @brief 深度バッファリソースを取得する
   /// @return 深度バッファリソース。未初期化の場合はnullptr
   ID3D12Resource* GetDepthBufferResource() const { return depthBuffer_.Get(); }

   /// @brief 深度バッファをシェーダーリソースとして読める状態に遷移
   void TransitionDepthStencilToShaderResource();

   /// @brief 深度バッファを書き込み可能なDSV状態に遷移
   void TransitionDepthStencilToWrite();

   /// @brief 深度バッファをコピー元として読める状態に遷移する
   void TransitionDepthStencilToCopySource();

   /// @brief コマンドリストを実行し、完了を待機
   /// @details コマンドリストを実行し、GPUが完了するまで待機
   void ExecuteCommandListAndWait();

   /// @brief SRVインデックスをインクリメント（フリーリスト使用時はpopのみ）
   void IncrementSrvIndex();

   /// @brief 次のSRVインデックスを取得（フリーリストがあればそちらを優先）
   /// @return 次のSRVインデックス
   UINT GetNextSrvIndex() const;

   /// @brief 使い終わったSRVインデックスをフリーリストに返却
   /// @param index 返却するインデックス
   void ReleaseSrvIndex(UINT index);

   /// @brief CBV/SRV/UAVのデスクリプタサイズを取得
   /// @return CBV/SRV/UAVのデスクリプタサイズ
   uint32_t GetDescriptorSizeCBVSRVUAV() const { return descriptorSizeCBVSRVUAV; };

   /// @brief RTVのデスクリプタサイズを取得
   /// @return RTVのデスクリプタサイズ
   uint32_t GetDescriptorSizeRTV() const { return descriptorSizeRTV; };

   /// @brief DSVのデスクリプタサイズを取得
   /// @return DSVのデスクリプタサイズ
   uint32_t GetDescriptorSizeDSV() const { return descriptorSizeDSV; };

   /// @brief RTVフォーマットを取得
   /// @return RTVフォーマット
   DXGI_FORMAT GetRTVFormat() const { return rtvFormat_; }

   /// @brief フルスクリーンを切り替える
   void ToggleFullscreen();

   /// @brief ウィンドウサイズにバックバッファサイズを同期する
   void SyncBackBufferSizeToWindow();

   /// @brief 現在のバックバッファ幅を取得
   uint32_t GetBackBufferWidth() const { return static_cast<uint32_t>(backBufferWidth_); }

   /// @brief 現在のバックバッファ高さを取得
   uint32_t GetBackBufferHeight() const { return static_cast<uint32_t>(backBufferHeight_); }

private:

   Window* window_ = nullptr;

   ComPtr<IDXGIFactory7> dxgiFactory_ = nullptr;
   ComPtr<ID3D12Device> device_ = nullptr;
   ComPtr<ID3D12GraphicsCommandList> commandList_ = nullptr;
   ComPtr<ID3D12CommandAllocator> commandAllocator_ = nullptr;
   ComPtr<ID3D12CommandQueue> commandQueue_ = nullptr;
   ComPtr<IDXGISwapChain4> swapChain_ = nullptr;
   std::vector<ComPtr<ID3D12Resource>> backBuffers_;
   ComPtr<ID3D12Resource> depthBuffer_ = nullptr;
   ComPtr<ID3D12DescriptorHeap> rtvHeap_ = nullptr;
   ComPtr<ID3D12DescriptorHeap> dsvHeap_ = nullptr;
   ComPtr<ID3D12DescriptorHeap> srvHeap_ = nullptr;
   D3D12_CPU_DESCRIPTOR_HANDLE depthSrvHandleCPU_ = {};
   D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandleGPU_ = {};
   ComPtr<ID3D12Fence> fence_ = nullptr;
   UINT64 fenceValue_ = 0;
   int32_t backBufferWidth_ = 0;
   int32_t backBufferHeight_ = 0;
   uint32_t descriptorSizeCBVSRVUAV = 0;
   uint32_t descriptorSizeRTV = 0;
   uint32_t descriptorSizeDSV = 0;
   UINT nextSrvIndex_ = 0;
   UINT depthSrvIndex_ = static_cast<UINT>(-1);
   D3D12_RESOURCE_STATES depthBufferState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
   std::queue<UINT> freeSrvIndices_;
   std::queue<std::filesystem::path> screenshotRequests_;

   UINT rtvCount_ = 4;
   DXGI_FORMAT rtvFormat_ = DXGI_FORMAT_R8G8B8A8_UNORM;

   std::chrono::steady_clock::time_point reference_;
private:
   /// @brief デバッグレイヤーを有効
   void EnableDebugLayer();

   /// @brief DXGIデバイスの初期化
   /// @param enableDebugLayer デバッグレイヤーを有効にするかどうか
   void InitializeDXGIDevice(bool enableDebugLayer = true);

   /// @brief コマンドアロケータ、コマンドリスト、コマンドキューの初期化
   void InitializeCommand();

   /// @brief スワップチェーンの作成
   void CreateSwapChain();

   /// @brief レンダーターゲットビューの作成
   void CreateRenderTargetViews();

   /// @brief 深度ステンシルビューの作成
   void CreateDepthStencilViews();

   /// @brief SRVヒープの作成
   void CreateSRVHeap();

   /// @brief フェンスの作成
   void CreateFence();

   /// @brief 固定FPSの初期化
   void InitializeFixFPS();

   /// @brief 固定FPSの更新
   void UpdateFixFPS();

   /// @brief コマンドリストを実行し、完了を待機
   ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

   /// @brief スワップチェーン関連リソースを指定サイズへリサイズ
   void ResizeSwapChainResources(uint32_t width, uint32_t height);
};
}
