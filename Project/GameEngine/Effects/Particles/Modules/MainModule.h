#pragma once

#include "GameEngine/Math/VectorMath.h"
#include "GameEngine/Math/MathUtils.h"
#include <nlohmann/json.hpp>
#include "GameEngine/Effects/Particles/Modules/ParticleModule.h"
#include <algorithm>
#include <cmath>

namespace GameEngine {
// ============================================================
// 乱数範囲構造体
// ============================================================

/// @brief ランダム範囲を表す構造体（浮動小数点数）
struct RandomFloat {

   /// @brief ゼロを返す固定範囲を構築する
   RandomFloat() = default;

   /// @brief 指定値を有限値として保持する固定範囲を構築する
   RandomFloat(float value) : RandomFloat(value, value, false) {}

   /// @brief 固定値ではlowだけを保持し、抽選時だけ範囲の両端を検証する
   RandomFloat(float low, float high, bool randomize = true);

   /// @brief 検証済みの下限を値として観測する
   float Minimum() const { return minimum_; }

   /// @brief 検証済みの上限を値として観測する
   float Maximum() const { return maximum_; }

   /// @brief 固定値ではなく範囲内から抽選するか確認する
   bool IsRandomized() const { return randomized_; }

   /// @brief 検証済み範囲から初期値を生成する
   float GetValue() const;

   /// @brief 範囲設定をJSONへ変換する
   nlohmann::json ToJson() const;

   /// @brief JSONを検証済み範囲として一括復元する
   void FromJson(const nlohmann::json& json);

private:
   float minimum_ = 0.0f;    ///< 固定値として、または乱数範囲の下限として使う値。
   float maximum_ = 0.0f;    ///< 乱数範囲の上限として使う値。
   bool randomized_ = false; ///< 固定値ではなく下限・上限の範囲から乱数を生成するかどうか。
};

/// @brief ランダム範囲を表す構造体（Vector2）
struct RandomVector2 {

   /// @brief ゼロを返す固定範囲を構築する
   RandomVector2() = default;

   /// @brief 指定値を有限値として保持する固定範囲を構築する
   RandomVector2(const Vector2& value) : RandomVector2(value, value, false) {}

   /// @brief 固定値ではlowだけを保持し、抽選時だけ範囲の両端を検証する
   RandomVector2(const Vector2& low, const Vector2& high, bool randomize = true);

   /// @brief 検証済みの下限を値として観測する
   Vector2 Minimum() const { return minimum_; }

   /// @brief 検証済みの上限を値として観測する
   Vector2 Maximum() const { return maximum_; }

   /// @brief 固定値ではなく範囲内から抽選するか確認する
   bool IsRandomized() const { return randomized_; }

   /// @brief 検証済み範囲から初期値を生成する
   Vector2 GetValue() const;

   /// @brief 範囲設定をJSONへ変換する
   nlohmann::json ToJson() const;

   /// @brief JSONを検証済み範囲として一括復元する
   void FromJson(const nlohmann::json& json);

private:
   Vector2 minimum_ = Vector2{0.0f, 0.0f}; ///< 固定値として、または乱数範囲の下限として使う値。
   Vector2 maximum_ = Vector2{0.0f, 0.0f}; ///< 乱数範囲の上限として使う値。
   bool randomized_ = false;               ///< 固定値ではなく下限・上限の範囲から乱数を生成するかどうか。
};

/// @brief ランダム範囲を表す構造体（Vector3）
struct RandomVector3 {

   /// @brief ゼロを返す固定範囲を構築する
   RandomVector3() = default;

   /// @brief 指定値を有限値として保持する固定範囲を構築する
   RandomVector3(const Vector3& value) : RandomVector3(value, value, false) {}

   /// @brief 固定値ではlowだけを保持し、抽選時だけ範囲の両端を検証する
   RandomVector3(const Vector3& low, const Vector3& high, bool randomize = true);

   /// @brief 検証済みの下限を値として観測する
   Vector3 Minimum() const { return minimum_; }

   /// @brief 検証済みの上限を値として観測する
   Vector3 Maximum() const { return maximum_; }

   /// @brief 固定値ではなく範囲内から抽選するか確認する
   bool IsRandomized() const { return randomized_; }

   /// @brief 検証済み範囲から初期値を生成する
   Vector3 GetValue() const;

   /// @brief 範囲設定をJSONへ変換する
   nlohmann::json ToJson() const;

   /// @brief JSONを検証済み範囲として一括復元する
   void FromJson(const nlohmann::json& json);

private:
   Vector3 minimum_ = Vector3{0.0f, 0.0f, 0.0f}; ///< 固定値として、または乱数範囲の下限として使う値。
   Vector3 maximum_ = Vector3{0.0f, 0.0f, 0.0f}; ///< 乱数範囲の上限として使う値。
   bool randomized_ = false;                     ///< 固定値ではなく下限・上限の範囲から乱数を生成するかどうか。
};

/// @brief ランダム範囲を表す構造体（色）
struct RandomColor {
   uint32_t minValue = 0xFFFFFFFF; ///< 固定色または色範囲の下限
   uint32_t maxValue = 0xFFFFFFFF; ///< 色範囲の上限
   bool randomize = false;         ///< RGBA各成分を範囲から抽選する場合はtrue

   /// @brief 不透明白を返す固定範囲を構築する
   RandomColor() = default;

   /// @brief 指定色を常に返す固定範囲を構築する
   /// @param value RGBA8形式の固定色
   RandomColor(uint32_t value) : minValue(value), maxValue(value), randomize(false) {}

   /// @brief RGBA8色の下限・上限と抽選有無を指定して構築する
   /// @param minVal 各成分の下限を持つRGBA8色
   /// @param maxVal 各成分の上限を持つRGBA8色
   /// @param enableRandom 各成分を範囲から抽選する場合はtrue
   RandomColor(uint32_t minVal, uint32_t maxVal, bool enableRandom = true);

   /// @brief 固定色または成分別の乱数色を取得する
   /// @return RGBA8形式の色
   uint32_t GetValue() const;

   /// @brief 色範囲設定をJSONへ変換する
   /// @return min・max・randomizeを含むJSON
   nlohmann::json ToJson() const;

   /// @brief JSONに含まれる色範囲設定を反映する
   /// @param json 読み込む設定
   void FromJson(const nlohmann::json& json);
};

/// @brief パーティクルシステム全体の寿命・初期値・再生条件を管理する
class MainModule {
public:
   /// @brief パーティクルの位置を保持・更新する座標空間
   enum class SimulationSpace {
      World, ///< 放出後はエミッター移動の影響を受けない
      Local  ///< 放出後もエミッターのローカル空間に追従する
   };

   /// @brief 初速の算出方法
   enum class StartSpeedMode {
      Directional, ///< Shapeが返す放出方向へスカラー速度を掛ける
      Vector3      ///< XYZ速度を直接使用する
   };

   /// @brief 一般的な連続放出に適した既定値で構築する
   MainModule();

   /// @brief 1回の放出サイクル時間を設定する
   /// @param duration サイクル時間（秒）
   void SetDuration(float duration) { duration_ = duration; }

   /// @brief 1回の放出サイクル時間を取得する
   /// @return サイクル時間（秒）
   float GetDuration() const { return duration_; }

   /// @brief サイクル終端から再び放出を続けるか設定する
   /// @param loop ループする場合はtrue
   void SetLooping(bool loop) { looping_ = loop; }

   /// @brief 放出サイクルがループするか取得する
   /// @return ループする場合はtrue
   bool IsLooping() const { return looping_; }

   /// @brief 生成時に選ぶ寿命範囲を設定する
   /// @param lifetime 寿命範囲（秒）
   void SetStartLifetime(const RandomFloat& lifetime) { startLifetime_ = lifetime; }

   /// @brief 生成時の寿命範囲を取得する
   /// @return 寿命範囲（秒）
   const RandomFloat& GetStartLifetime() const { return startLifetime_; }

   /// @brief 放出方向へ掛ける初速範囲を設定する
   /// @param speed スカラー速度範囲
   void SetStartSpeed(const RandomFloat& speed) { startSpeed_ = speed; }

   /// @brief 放出方向へ掛ける初速範囲を取得する
   /// @return スカラー速度範囲
   const RandomFloat& GetStartSpeed() const { return startSpeed_; }

   /// @brief 初速の算出方法を設定する
   /// @param mode 放出方向またはXYZ直接指定
   void SetStartSpeedMode(StartSpeedMode mode) { startSpeedMode_ = mode; }

   /// @brief 初速の算出方法を取得する
   /// @return 現在の初速モード
   StartSpeedMode GetStartSpeedMode() const { return startSpeedMode_; }

   /// @brief XYZ初速範囲を設定してVector3モードへ切り替える
   /// @param velocity 成分別の速度範囲
   void SetStartVelocity(const RandomVector3& velocity);

   /// @brief 固定XYZ初速を設定してVector3モードへ切り替える
   /// @param velocity 固定速度
   void SetStartVelocity(const Vector3& velocity) { SetStartVelocity(RandomVector3(velocity, velocity, false)); }

   /// @brief XYZ初速範囲を取得する
   /// @return 成分別の速度範囲
   const RandomVector3& GetStartVelocity() const { return startVelocity_; }

   /// @brief 生成時のXYZスケール範囲を設定する
   /// @param size 成分別のスケール範囲
   void SetStartSize(const RandomVector3& size) { startSize_ = size; }

   /// @brief 生成時のXYZスケール範囲を取得する
   /// @return 成分別のスケール範囲
   const RandomVector3& GetStartSize() const { return startSize_; }

   /// @brief 生成時のXYZ回転範囲を設定する
   /// @param rotation 成分別の回転範囲
   void SetStartRotation(const RandomVector3& rotation) { startRotation_ = rotation; }

   /// @brief 生成時のXYZ回転範囲を取得する
   /// @return 成分別の回転範囲
   const RandomVector3& GetStartRotation() const { return startRotation_; }

   /// @brief 生成時のRGBA8色範囲を設定する
   /// @param color 色範囲
   void SetStartColor(const RandomColor& color) { startColor_ = color; }

   /// @brief 生成時のRGBA8色範囲を取得する
   /// @return 色範囲
   const RandomColor& GetStartColor() const { return startColor_; }

   /// @brief 全パーティクルで共通の重力倍率を設定する
   /// @param modifier 重力加速度へ掛ける倍率
   void SetGravityModifier(float modifier) { gravityModifier_ = RandomFloat(modifier, modifier, false); }

   /// @brief 固定設定時の重力倍率を取得する
   /// @return 重力倍率範囲の下限
   float GetGravityModifier() const { return gravityModifier_.Minimum(); }

   /// @brief 粒子ごとの重力倍率範囲を設定する
   void SetGravityModifierRange(const RandomFloat& modifier) { gravityModifier_ = modifier; }

   /// @brief 粒子ごとの重力倍率範囲を取得する
   const RandomFloat& GetGravityModifierRange() const { return gravityModifier_; }

   /// @brief このシステム固有の時間倍率を設定する
   /// @param timeScale 0で停止、1で等速となる0以上の倍率
   void SetTimeScale(float timeScale) { timeScale_ = std::max(timeScale, 0.0f); }

   /// @brief このシステム固有の時間倍率を取得する
   /// @return シミュレーション時間へ掛ける0以上の倍率
   float GetTimeScale() const { return timeScale_; }

   /// @brief パーティクルのシミュレーション座標空間を設定する
   /// @param space ワールドまたはローカル空間
   void SetSimulationSpace(SimulationSpace space) { simulationSpace_ = space; }

   /// @brief パーティクルのシミュレーション座標空間を取得する
   /// @return 現在の座標空間
   SimulationSpace GetSimulationSpace() const { return simulationSpace_; }

   /// @brief システム初期化時に自動再生するか設定する
   /// @param play 自動再生する場合はtrue
   void SetPlayOnAwake(bool play) { playOnAwake_ = play; }

   /// @brief システム初期化時に自動再生するか取得する
   /// @return 自動再生する場合はtrue
   bool GetPlayOnAwake() const { return playOnAwake_; }

   /// @brief 同時に生存できるパーティクル数の上限を設定する
   /// @param max 最大パーティクル数
   void SetMaxParticles(uint32_t max) { maxParticles_ = max; }

   /// @brief 同時に生存できるパーティクル数の上限を取得する
   /// @return 最大パーティクル数
   uint32_t GetMaxParticles() const { return maxParticles_; }

   /// @brief メインモジュール設定をJSONへ変換する
   /// @return 現在の全設定を含むJSON
   nlohmann::json ToJson() const;

   /// @brief JSONに含まれるメインモジュール設定を反映する
   /// @param json 読み込む設定
   void FromJson(const nlohmann::json& json);

#ifdef USE_IMGUI

   /// @brief パーティクル全体設定を編集するインスペクターを描画する
   void DrawInspector();
#endif

private:
   float duration_ = 5.0f; ///< 1回の放出サイクル時間（秒）。
   bool looping_ = true;   ///< 放出サイクルがループするかどうかする。

   RandomFloat startLifetime_{2.0f, 2.5f, false};                ///< 生成時の寿命範囲（秒）。
   RandomFloat startSpeed_{5.0f, 6.0f, false};                   ///< 放出方向へ掛ける初速範囲。
   StartSpeedMode startSpeedMode_ = StartSpeedMode::Directional; ///< 初速の算出方法。
   RandomVector3 startVelocity_{Vector3(0.0f, 5.0f, 0.0f), Vector3(0.0f, 6.0f, 0.0f), false}; ///< XYZ初速範囲。
   /// @brief 生成時のXYZスケール範囲。
   RandomVector3 startSize_{Vector3(1.0f, 1.0f, 1.0f), Vector3(1.0f, 1.0f, 1.0f), false};
   /// @brief 生成時のXYZ回転範囲。
   RandomVector3 startRotation_{Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), false};
   RandomColor startColor_{0xFFFFFFFF, 0xFFFFFFFF, false}; ///< 生成時のRGBA8色範囲。

   RandomFloat gravityModifier_{0.0f, 0.0f, false};           ///< 粒子ごとの重力倍率範囲。
   float timeScale_ = 1.0f;                                   ///< このシステム固有の時間倍率。
   SimulationSpace simulationSpace_ = SimulationSpace::World; ///< パーティクルのシミュレーション座標空間。
   bool playOnAwake_ = true;                                  ///< システム初期化時に自動再生するかどうかする。
   uint32_t maxParticles_ = 1000;                             ///< 同時に生存できるパーティクル数の上限。
};
}
