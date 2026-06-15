#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <utility>
#include "Window/Window.h"

namespace GameEngine {
class GraphicsDevice;

/// @brief オフスクリーンレンダリングターゲットクラス
class OffscreenRenderTarget {
public:
   /// @brief オフスクリーンレンダリングターゲットの初期化
   /// @param device グラフィックスデバイス
   /// @param width ウィンドウの幅
   /// @param height ウィンドウの高さ
   void Initialize(GraphicsDevice* device, uint32_t width = Window::kResolutionWidth, uint32_t height = Window::kResolutionHeight);

   /// @brief オフスクリーンレンダリングターゲットを指定サイズへリサイズ
   /// @param width 新しい幅
   /// @param height 新しい高さ
   void Resize(uint32_t width, uint32_t height);

   /// @brief 描画前の処理
   /// @param useDSV 深度ステンシルビューを使用するかどうか
   void PreDraw(bool useDSV);

   /// @brief 描画前の処理（クリアなし）
   /// @param useDSV 深度ステンシルビューを使用するかどうか
   void PreDrawWithoutClear(bool useDSV);

   /// @brief 描画後の処理
   void PostDraw();

   /// @brief 現在のSRVハンドル（GPU）を取得
   D3D12_GPU_DESCRIPTOR_HANDLE GetSRVHandleGPU() const { return currentRenderTarget_.srvHandleGPU; }

   /// @brief 前フレームのSRVハンドル（GPU）を取得
   D3D12_GPU_DESCRIPTOR_HANDLE GetPreviousSRVHandleGPU() const { return previousRenderTarget_.srvHandleGPU; }

   /// @brief 現在のリソースを取得
   ID3D12Resource* GetResource() const { return currentRenderTarget_.renderTarget.Get(); }

   /// @brief 前フレームのリソースを取得
   ID3D12Resource* GetPreviousResource() const { return previousRenderTarget_.renderTarget.Get(); }

   /// @brief レンダーターゲットを交換
   void SwapBuffers() {
	  std::swap(currentRenderTarget_, previousRenderTarget_);
   }

   /// @brief オフスクリーンレンダリングターゲットの幅を取得
   uint32_t GetWidth() const { return width_; }

   /// @brief オフスクリーンレンダリングターゲットの高さを取得
   uint32_t GetHeight() const { return height_; }

   /// @brief レンダーターゲットのフォーマットを取得
   /// @return フォーマット
   DXGI_FORMAT GetFormat() const { return format_; }

private:
   /// @brief レンダーターゲットの情報をまとめた構造体
   struct RenderTargetInfo {
	  Microsoft::WRL::ComPtr<ID3D12Resource> renderTarget;
	  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
	  D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
	  D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
	  UINT srvIndex = static_cast<UINT>(-1);
   };

   GraphicsDevice* device_ = nullptr;

   uint32_t width_ = 0;
   uint32_t height_ = 0;
   float clearColor_[4] = { 0.1f,0.25f,0.5f,1.0f };

   // レンダーターゲット情報
   RenderTargetInfo currentRenderTarget_;
   RenderTargetInfo previousRenderTarget_;

   DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM;

   /// @brief レンダーターゲット情報を作成
   /// @param index 作成するレンダーターゲットのインデックス（0または1）
   /// @param srvIndex 再利用するSRVインデックス。未指定の場合は新規確保
   /// @return 作成されたレンダーターゲット情報
   RenderTargetInfo CreateRenderTargetInfo(int index, UINT srvIndex = static_cast<UINT>(-1));
};
}
