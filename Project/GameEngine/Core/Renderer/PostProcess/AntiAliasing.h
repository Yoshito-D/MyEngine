#pragma once
#include "PostProcess.h"
#include <d3d12.h>
#include <wrl.h>

namespace GameEngine {
/// @brief 輝度差からエッジ方向を推定して平滑化するスクリーンスペースアンチエイリアシング
class AntiAliasing : public PostProcess {
public:
   /// @brief HLSLのAntiAliasingParamsと同じ定数バッファレイアウト
   struct AntiAliasingCB {
	  float contrastThreshold; ///< エッジと判定する最小絶対輝度差
	  float relativeThreshold; ///< 周辺最大輝度に対する相対しきい値
	  float subpixelBlending;  ///< 元色と平滑化色の補間率
	  float edgeSearchSteps;   ///< エッジ方向へ許可する最大探索距離
   };

   /// @brief 共有描画環境とアンチエイリアシング用定数バッファを初期化する。
   /// @param device グラフィックスデバイス
   /// @param renderTarget ポストプロセスの出力先
   void Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) override;

   /// @brief 入力カラーへ輝度ベースの方向性フィルターを適用する。
   /// @param inputSRV 入力カラーのSRV
   void Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) override;

#ifdef USE_IMGUI
   /// @brief アンチエイリアシング設定のエディターUIを描画する。
   void ImGuiEdit() override;
#endif

   /// @brief エディター表示と保存識別に使用するエフェクト名を取得する。
   /// @return エフェクト名
   const char* GetEffectName() const override { return "Anti Aliasing"; }

   /// @copydoc PostProcess::SerializeSettings
   nlohmann::json SerializeSettings() const override;
   /// @copydoc PostProcess::DeserializeSettings
   bool DeserializeSettings(const nlohmann::json& settings) override;

   /// @brief エッジ検出に必要な最小絶対輝度差を設定する。
   /// @note 値はCPU側でクランプされない。
   /// @param threshold 絶対コントラストしきい値
   void SetContrastThreshold(float threshold) { contrastThreshold_ = threshold; UpdateConstantBuffer(); }

   /// @brief 周辺最大輝度に対する相対コントラストしきい値を設定する。
   /// @note 値はCPU側でクランプされない。
   /// @param threshold 相対コントラストしきい値
   void SetRelativeThreshold(float threshold) { relativeThreshold_ = threshold; UpdateConstantBuffer(); }

   /// @brief 元色と平滑化色の補間率を設定する。
   /// @param blending 補間率。シェーダー側で0～1へ飽和される
   void SetSubpixelBlending(float blending) { subpixelBlending_ = blending; UpdateConstantBuffer(); }

   /// @brief 推定したエッジ方向へ許可する最大探索距離を設定する。
   /// @param steps 最大距離。シェーダー側で1以上として扱われる
   void SetEdgeSearchSteps(float steps) { edgeSearchSteps_ = steps; UpdateConstantBuffer(); }

private:
   float contrastThreshold_ = 0.0312f;
   float relativeThreshold_ = 0.125f;
   float subpixelBlending_ = 0.75f;
   float edgeSearchSteps_ = 8.0f;

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
   AntiAliasingCB* constantBufferData_ = nullptr;

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
};
}
