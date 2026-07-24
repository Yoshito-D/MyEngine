#pragma once
#include "ParticleModule.h"
#include "MainModule.h"
#include "Utility/VectorMath.h"
#include <algorithm>
#include <string>

namespace GameEngine {
/// @brief パーティクルの移動履歴をトレイルとして描画する設定を管理する
class TrailModule final : public ParticleModule {
public:
   /// @brief トレイルを生成する位置の扱い
   enum class TrailMode {
	  ParticlePath = 0,
	  EmitterToParticle
   };

   /// @brief トレイルモジュールを既定値で構築する
   TrailModule() = default;

   /// @brief トレイルの生成方式を設定する
   /// @param mode 粒子の移動軌跡、またはエミッターから粒子までの接続
   void SetMode(TrailMode mode) { mode_ = mode; }

   /// @brief トレイルの生成方式を取得する
   /// @return 現在の生成方式
   TrailMode GetMode() const { return mode_; }

   /// @brief 粒子ごとのトレイル幅範囲を設定する
   /// @param width 幅の固定値またはランダム範囲
   void SetWidthRange(const RandomFloat& width) { width_ = width; }

   /// @brief 粒子ごとのトレイル幅範囲を取得する
   /// @return 幅の固定値またはランダム範囲
   const RandomFloat& GetWidthRange() const { return width_; }

   /// @brief 1粒子が保持する履歴点数を設定する
   /// @param count 2から128の履歴点数
   void SetMaxPoints(uint32_t count) { maxPoints_ = std::clamp(count, 2u, 128u); }

   /// @brief 1粒子が保持する履歴点数を取得する
   /// @return 履歴点数
   uint32_t GetMaxPoints() const { return maxPoints_; }

   /// @brief 履歴点を追加する最小移動距離を設定する
   /// @param distance 0より大きいワールド距離
   void SetMinDistance(float distance) { minDistance_ = std::max(distance, 0.0001f); }

   /// @brief 履歴点を追加する最小移動距離を取得する
   /// @return ワールド距離
   float GetMinDistance() const { return minDistance_; }

   /// @brief トレイル専用テクスチャ名を設定する
   /// @param textureName 空文字の場合はパーティクル本体のテクスチャを使用する
   void SetTextureName(const std::string& textureName) { textureName_ = textureName; }

   /// @brief トレイル専用テクスチャ名を取得する
   /// @return テクスチャ名
   const std::string& GetTextureName() const { return textureName_; }

   /// @brief トレイル全体に乗算する色を設定する
   /// @param color RGBAカラー
   void SetColor(const Vector4& color) { color_ = color; }

   /// @brief トレイル全体に乗算する色を取得する
   /// @return RGBAカラー
   const Vector4& GetColor() const { return color_; }

   /// @brief 親粒子の消滅後に末尾が先端へ到達する時間を設定する
   /// @param duration 巻き取り時間（秒）
   void SetRetractionDuration(float duration) { retractionDuration_ = std::max(duration, 0.0f); }

   /// @brief 親粒子の消滅後に末尾が先端へ到達する時間を取得する
   /// @return 巻き取り時間（秒）
   float GetRetractionDuration() const { return retractionDuration_; }

   /// @brief トレイル末尾の幅倍率を設定する
   /// @param scale 0で先細り、1で全長を同じ幅にする
   void SetTailWidthScale(float scale) { tailWidthScale_ = std::clamp(scale, 0.0f, 1.0f); }

   /// @brief トレイル末尾の幅倍率を取得する
   /// @return 0から1の幅倍率
   float GetTailWidthScale() const { return tailWidthScale_; }

   /// @brief トレイル全長に対するテクスチャの繰り返し回数を設定する
   /// @param tiling 1から64の繰り返し回数
   void SetTextureTiling(float tiling) { textureTiling_ = std::clamp(tiling, 1.0f, 64.0f); }

   /// @brief トレイル全長に対するテクスチャの繰り返し回数を取得する
   /// @return 繰り返し回数
   float GetTextureTiling() const { return textureTiling_; }

   /// @brief トレイル設定をJSONへ変換する
   /// @return トレイル設定JSON
   nlohmann::json ToJson() const override;

   /// @brief 新形式または旧RendererModule形式のJSONから設定を読み込む
   /// @param json 設定JSON
   void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI
   /// @brief トレイル専用インスペクターを描画する
   void DrawInspector() override;
#endif

private:
   TrailMode mode_ = TrailMode::ParticlePath;
   RandomFloat width_{ 0.5f, 0.5f, false };
   uint32_t maxPoints_ = 16;
   float minDistance_ = 0.1f;
   std::string textureName_;
   Vector4 color_{ 1.0f, 1.0f, 1.0f, 1.0f };
   float retractionDuration_ = 0.5f;
   float tailWidthScale_ = 0.0f;
   float textureTiling_ = 1.0f;
};
}
