#pragma once

#include "GameEngine/Graphics/PostProcess/PostProcess.h"
#include "GameEngine/Graphics/Device/ResourceHelper.h"
#include "GameEngine/Math/Types/Vector2.h"
#include <wrl.h>
#include <d3d12.h>
#include <algorithm>

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace GameEngine {

/// @brief 集中線シェーダーへ渡す調整パラメーター
struct SpeedLineParams {
   Vector2 center = {0.5f, 0.5f}; ///< 集中線の中心を表す正規化UV座標
   float intensity = 0.75f;       ///< 線の明るさ
   float lineDensity = 180.0f;    ///< 放射方向へ生成する線の密度
   float thickness = 0.85f;       ///< 線の太さ
   float innerRadius = 0.10f;     ///< 線を描き始める内側半径
   float outerRadius = 1.00f;     ///< 線を描き終える外側半径
   float time = 0.0f;             ///< アニメーション時刻
   float randomSeed = 1.0f;       ///< 線パターンを選ぶ乱数シード
   float flowSpeed = 1.0f;        ///< 中心から外側へ流れる速度
};

/// @brief 集中線効果
class SpeedLine : public PostProcess {
public:
   /// @brief 集中線用定数バッファ構造体
   struct SpeedLineCB {
      Vector2 center; ///< 中心点。

      float intensity;   ///< 効果または光の強さ。
      float lineDensity; ///< 放射方向へ生成する線の密度。

      float thickness;   ///< 深度近傍のサンプリング間隔倍率。
      float innerRadius; ///< 線を描き始める内側半径。

      float outerRadius; ///< 線を描き終える外側半径。
      float time;        ///< クリップ先頭からの時刻（秒）。

      float randomSeed; ///< 線パターンを選ぶ乱数シード。
      float flowSpeed;  ///< 中心から外側へ流れる速度。

      float padding[2]; ///< GPU転送データの配置境界を揃える未使用領域。
   };

   /// @brief 初期化
   /// @param device グラフィックスデバイス
   /// @param renderTarget レンダーターゲット
   void Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) override;

   /// @brief エフェクトを適用
   /// @param inputSRV 入力SRV
   void Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) override;

#ifdef USE_IMGUI

   /// @brief 集中線の密度・範囲・太さ・強度・流速などをエディターで編集する。
   void ImGuiEdit() override;
#endif

   /// @brief エディター表示と保存識別に使うエフェクト名を返す。
   /// @return 「Speed Line」を表す固定文字列。
   const char* GetEffectName() const override { return "Speed Line"; }

   /// @brief 集中線の中心・密度・太さ・範囲・時刻・流速をシーン保存用JSONへ変換する。
   /// @return 当該エフェクト固有の設定を含むJSONオブジェクト。
   nlohmann::json SerializeSettings() const override;

   /// @brief 集中線の中心・密度・太さ・範囲・時刻・流速を部分設定JSONから復元し、GPU定数へ反映する。
   /// @param settings エフェクトの設定JSONオブジェクト。未指定・型不正の項目は現在値を使う。
   /// @return JSONオブジェクトを受け付けた場合はtrue。それ以外はfalse。
   bool DeserializeSettings(const nlohmann::json& settings) override;

   /// @brief 集中線パラメーターを検証してまとめて設定する
   void SetParams(const SpeedLineParams& params);

   /// @brief 現在の集中線パラメーターを取得する
   const SpeedLineParams& GetParams() const { return params_; }

   /// @brief 集中線の中心を設定する
   void SetCenter(Vector2 center);

   /// @brief 集中線の明るさを設定する
   void SetIntensity(float value);

   /// @brief 集中線の密度を設定する
   void SetLineDensity(float value);

private:
   SpeedLineParams params_; ///< 現在の集中線パラメーター。

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_; ///< この処理の設定をGPUへ渡す所有定数バッファ。
   SpeedLineCB* constantBufferData_ = nullptr; ///< 定数バッファの永続Map先。対応するリソースがメモリを所有する。

   void CreateConstantBuffer();

   void UpdateConstantBuffer();
};
}
