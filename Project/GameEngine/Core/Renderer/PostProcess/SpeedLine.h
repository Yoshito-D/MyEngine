#pragma once
#include "PostProcess.h"
#include "ResourceHelper.h"
#include "Utility/Math/Vector2.h"
#include <wrl.h>
#include <d3d12.h>
#include <algorithm>

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace GameEngine {
/// @brief 集中線シェーダーへ渡す調整パラメーター
struct SpeedLineParams {
   Vector2 center = { 0.5f, 0.5f }; ///< 集中線の中心を表す正規化UV座標
   float intensity = 0.75f; ///< 線の明るさ
   float lineDensity = 180.0f; ///< 放射方向へ生成する線の密度
   float thickness = 0.85f; ///< 線の太さ
   float innerRadius = 0.10f; ///< 線を描き始める内側半径
   float outerRadius = 1.00f; ///< 線を描き終える外側半径
   float time = 0.0f; ///< アニメーション時刻
   float randomSeed = 1.0f; ///< 線パターンを選ぶ乱数シード
   float flowSpeed = 1.0f; ///< 中心から外側へ流れる速度
};

/// @brief 集中線効果
class SpeedLine : public PostProcess {
public:
   /// @brief 集中線用定数バッファ構造体
   struct SpeedLineCB
   {
	  Vector2 center;

	  float intensity;
	  float lineDensity;

	  float thickness;
	  float innerRadius;

	  float outerRadius;
	  float time;

	  float randomSeed;
	  float flowSpeed;

	  float padding[2];
   };

   /// @brief 初期化
   /// @param device グラフィックスデバイス
   /// @param renderTarget レンダーターゲット
   void Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) override;

   /// @brief エフェクトを適用
   /// @param inputSRV 入力SRV
   void Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) override;

#ifdef USE_IMGUI
   /// @copydoc PostProcess::ImGuiEdit
   void ImGuiEdit() override;
#endif
   /// @copydoc PostProcess::GetEffectName
   const char* GetEffectName() const override { return "Speed Line"; }

   /// @copydoc PostProcess::SerializeSettings
   nlohmann::json SerializeSettings() const override;
   /// @copydoc PostProcess::DeserializeSettings
   bool DeserializeSettings(const nlohmann::json& settings) override;

   /// @brief 集中線パラメーターを検証してまとめて設定する
   void SetParams(const SpeedLineParams& params);
   /// @brief 現在の集中線パラメーターを取得する
   const SpeedLineParams& GetParams() const { return params_; }

   /// @brief 集中線の中心を設定する
   void SetCenter(Vector2 center) { auto params = params_; params.center = center; SetParams(params); }
   /// @brief 集中線の明るさを設定する
   void SetIntensity(float value) { auto params = params_; params.intensity = value; SetParams(params); }
   /// @brief 集中線の密度を設定する
   void SetLineDensity(float value) { auto params = params_; params.lineDensity = value; SetParams(params); }

private:
   SpeedLineParams params_;

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
   SpeedLineCB* constantBufferData_ = nullptr;

   void CreateConstantBuffer();

   void UpdateConstantBuffer();
};
}
