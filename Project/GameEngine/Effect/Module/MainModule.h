#pragma once
#include "Utility/VectorMath.h"
#include "Utility/MathUtils.h"
#include <nlohmann/json.hpp>
#include "ParticleModule.h"
#include <algorithm>

namespace GameEngine {
// ============================================================
// Random Range Structures
// ============================================================

/// @brief ランダム範囲を表す構造体（浮動小数点数）
struct RandomFloat {
   float minValue = 0.0f; ///< 固定値または乱数範囲の下限
   float maxValue = 0.0f; ///< 乱数範囲の上限
   bool randomize = false; ///< minValueとmaxValueの間から抽選する場合はtrue

   /// @brief 0を返す固定範囲を構築する
   RandomFloat() = default;
   /// @brief 指定値を常に返す固定範囲を構築する
   /// @param value 固定値
   RandomFloat(float value) : minValue(value), maxValue(value), randomize(false) {}
   /// @brief 下限・上限と抽選有無を指定して構築する
   /// @param minVal 範囲の下限
   /// @param maxVal 範囲の上限
   /// @param enableRandom 範囲から抽選する場合はtrue
   RandomFloat(float minVal, float maxVal, bool enableRandom = true)
	  : minValue(minVal), maxValue(maxVal), randomize(enableRandom) {}

   /// @brief 固定値または範囲内の乱数を取得する
   /// @return randomizeがtrueなら抽選値、それ以外はminValue
   float GetValue() const;

   /// @brief 範囲設定をJSONへ変換する
   /// @return min・max・randomizeを含むJSON
   nlohmann::json ToJson() const;
   /// @brief JSONに含まれる範囲設定を反映する
   /// @param json 読み込む設定
   void FromJson(const nlohmann::json& json);
};

/// @brief ランダム範囲を表す構造体（Vector2）
struct RandomVector2 {
   Vector2 minValue = Vector2{ 0.0f, 0.0f }; ///< 固定値または成分別乱数範囲の下限
   Vector2 maxValue = Vector2{ 0.0f, 0.0f }; ///< 成分別乱数範囲の上限
   bool randomize = false;                    ///< 各成分を範囲から抽選する場合はtrue

   /// @brief ゼロベクトルを返す固定範囲を構築する
   RandomVector2() = default;
   /// @brief 指定ベクトルを常に返す固定範囲を構築する
   /// @param value 固定値
   RandomVector2(const Vector2& value) : minValue(value), maxValue(value), randomize(false) {}
   /// @brief 成分別の下限・上限と抽選有無を指定して構築する
   /// @param minVal 範囲の下限
   /// @param maxVal 範囲の上限
   /// @param enableRandom 各成分を範囲から抽選する場合はtrue
   RandomVector2(const Vector2& minVal, const Vector2& maxVal, bool enableRandom = true)
	  : minValue(minVal), maxValue(maxVal), randomize(enableRandom) {}

   /// @brief 固定ベクトルまたは成分別の乱数を取得する
   /// @return randomizeがtrueなら抽選値、それ以外はminValue
   Vector2 GetValue() const;

   /// @brief 範囲設定をJSONへ変換する
   /// @return min・max・randomizeを含むJSON
   nlohmann::json ToJson() const;
   /// @brief JSONに含まれる範囲設定を反映する
   /// @param json 読み込む設定
   void FromJson(const nlohmann::json& json);
};

/// @brief ランダム範囲を表す構造体（Vector3）
struct RandomVector3 {
   Vector3 minValue = Vector3(0.0f, 0.0f, 0.0f); ///< 固定値または成分別乱数範囲の下限
   Vector3 maxValue = Vector3(0.0f, 0.0f, 0.0f); ///< 成分別乱数範囲の上限
   bool randomize = false;                         ///< 各成分を範囲から抽選する場合はtrue

   /// @brief ゼロベクトルを返す固定範囲を構築する
   RandomVector3() = default;
   /// @brief 指定ベクトルを常に返す固定範囲を構築する
   /// @param value 固定値
   RandomVector3(const Vector3& value) : minValue(value), maxValue(value), randomize(false) {}
   /// @brief 成分別の下限・上限と抽選有無を指定して構築する
   /// @param minVal 範囲の下限
   /// @param maxVal 範囲の上限
   /// @param enableRandom 各成分を範囲から抽選する場合はtrue
   RandomVector3(const Vector3& minVal, const Vector3& maxVal, bool enableRandom = true)
	  : minValue(minVal), maxValue(maxVal), randomize(enableRandom) {}

   /// @brief 固定ベクトルまたは成分別の乱数を取得する
   /// @return randomizeがtrueなら抽選値、それ以外はminValue
   Vector3 GetValue() const;

   /// @brief 範囲設定をJSONへ変換する
   /// @return min・max・randomizeを含むJSON
   nlohmann::json ToJson() const;
   /// @brief JSONに含まれる範囲設定を反映する
   /// @param json 読み込む設定
   void FromJson(const nlohmann::json& json);
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
   RandomColor(uint32_t minVal, uint32_t maxVal, bool enableRandom = true)
	  : minValue(minVal), maxValue(maxVal), randomize(enableRandom) {}

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
	  World,  ///< 放出後はエミッター移動の影響を受けない
	  Local   ///< 放出後もエミッターのローカル空間に追従する
   };

   /// @brief 初速の算出方法
   enum class StartSpeedMode {
	  Directional,  ///< Shapeが返す放出方向へスカラー速度を掛ける
	  Vector3       ///< XYZ速度を直接使用する
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
   /// @brief 生成時寿命の下限を設定する
   /// @param min 下限（秒）
   void SetStartLifetimeMin(float min) { startLifetime_.minValue = min; }
   /// @brief 生成時寿命の上限を設定する
   /// @param max 上限（秒）
   void SetStartLifetimeMax(float max) { startLifetime_.maxValue = max; }
   /// @brief 寿命を範囲から抽選するか設定する
   /// @param randomize 抽選する場合はtrue
   void SetStartLifetimeRandomize(bool randomize) { startLifetime_.randomize = randomize; }

   /// @brief 放出方向へ掛ける初速範囲を設定する
   /// @param speed スカラー速度範囲
   void SetStartSpeed(const RandomFloat& speed) { startSpeed_ = speed; }
   /// @brief XYZ初速範囲を設定してVector3モードへ切り替える
   /// @param velocity 成分別の速度範囲
   void SetStartSpeed(const RandomVector3& velocity) { SetStartVelocity(velocity); }
   /// @brief 固定XYZ初速を設定してVector3モードへ切り替える
   /// @param velocity 固定速度
   void SetStartSpeed(const Vector3& velocity) { SetStartVelocity(velocity); }
   /// @brief 放出方向へ掛ける初速範囲を取得する
   /// @return スカラー速度範囲
   const RandomFloat& GetStartSpeed() const { return startSpeed_; }
   /// @brief スカラー初速の下限を設定する
   /// @param min 下限速度
   void SetStartSpeedMin(float min) { startSpeed_.minValue = min; }
   /// @brief スカラー初速の上限を設定する
   /// @param max 上限速度
   void SetStartSpeedMax(float max) { startSpeed_.maxValue = max; }
   /// @brief スカラー初速を範囲から抽選するか設定する
   /// @param randomize 抽選する場合はtrue
   void SetStartSpeedRandomize(bool randomize) { startSpeed_.randomize = randomize; }
   /// @brief 初速の算出方法を設定する
   /// @param mode 放出方向またはXYZ直接指定
   void SetStartSpeedMode(StartSpeedMode mode) { startSpeedMode_ = mode; }
   /// @brief 初速の算出方法を取得する
   /// @return 現在の初速モード
   StartSpeedMode GetStartSpeedMode() const { return startSpeedMode_; }
   /// @brief XYZ初速範囲を設定してVector3モードへ切り替える
   /// @param velocity 成分別の速度範囲
   void SetStartVelocity(const RandomVector3& velocity) { startVelocity_ = velocity; startSpeedMode_ = StartSpeedMode::Vector3; }
   /// @brief 固定XYZ初速を設定してVector3モードへ切り替える
   /// @param velocity 固定速度
   void SetStartVelocity(const Vector3& velocity) { SetStartVelocity(RandomVector3(velocity, velocity, false)); }
   /// @brief XYZ初速範囲を取得する
   /// @return 成分別の速度範囲
   const RandomVector3& GetStartVelocity() const { return startVelocity_; }
   /// @brief 互換名でXYZ初速範囲を取得する
   /// @return 成分別の速度範囲
   const RandomVector3& GetStartSpeedVector() const { return startVelocity_; }
   /// @brief XYZ初速の下限を設定してVector3モードへ切り替える
   /// @param min 成分別の下限速度
   void SetStartVelocityMin(const Vector3& min) { startVelocity_.minValue = min; startSpeedMode_ = StartSpeedMode::Vector3; }
   /// @brief XYZ初速の上限を設定してVector3モードへ切り替える
   /// @param max 成分別の上限速度
   void SetStartVelocityMax(const Vector3& max) { startVelocity_.maxValue = max; startSpeedMode_ = StartSpeedMode::Vector3; }
   /// @brief XYZ初速を範囲から抽選するか設定する
   /// @param randomize 抽選する場合はtrue
   void SetStartVelocityRandomize(bool randomize) { startVelocity_.randomize = randomize; startSpeedMode_ = StartSpeedMode::Vector3; }

   /// @brief 生成時のXYZスケール範囲を設定する
   /// @param size 成分別のスケール範囲
   void SetStartSize(const RandomVector3& size) { startSize_ = size; }
   /// @brief 生成時のXYZスケール範囲を取得する
   /// @return 成分別のスケール範囲
   const RandomVector3& GetStartSize() const { return startSize_; }
   /// @brief 生成時スケールの下限を設定する
   /// @param min 成分別の下限
   void SetStartSizeMin(const Vector3& min) { startSize_.minValue = min; }
   /// @brief 生成時スケールの上限を設定する
   /// @param max 成分別の上限
   void SetStartSizeMax(const Vector3& max) { startSize_.maxValue = max; }
   /// @brief 生成時スケールを範囲から抽選するか設定する
   /// @param randomize 抽選する場合はtrue
   void SetStartSizeRandomize(bool randomize) { startSize_.randomize = randomize; }

   /// @brief 生成時のXYZ回転範囲を設定する
   /// @param rotation 成分別の回転範囲
   void SetStartRotation(const RandomVector3& rotation) { startRotation_ = rotation; }
   /// @brief 生成時のXYZ回転範囲を取得する
   /// @return 成分別の回転範囲
   const RandomVector3& GetStartRotation() const { return startRotation_; }
   /// @brief 生成時回転の下限を設定する
   /// @param min 成分別の下限
   void SetStartRotationMin(const Vector3& min) { startRotation_.minValue = min; }
   /// @brief 生成時回転の上限を設定する
   /// @param max 成分別の上限
   void SetStartRotationMax(const Vector3& max) { startRotation_.maxValue = max; }
   /// @brief 生成時回転を範囲から抽選するか設定する
   /// @param randomize 抽選する場合はtrue
   void SetStartRotationRandomize(bool randomize) { startRotation_.randomize = randomize; }

   /// @brief 生成時のRGBA8色範囲を設定する
   /// @param color 色範囲
   void SetStartColor(const RandomColor& color) { startColor_ = color; }
   /// @brief 生成時のRGBA8色範囲を取得する
   /// @return 色範囲
   const RandomColor& GetStartColor() const { return startColor_; }
   /// @brief 生成時色の成分別下限を設定する
   /// @param min RGBA8形式の下限色
   void SetStartColorMin(uint32_t min) { startColor_.minValue = min; }
   /// @brief 生成時色の成分別上限を設定する
   /// @param max RGBA8形式の上限色
   void SetStartColorMax(uint32_t max) { startColor_.maxValue = max; }
   /// @brief 生成時色を範囲から抽選するか設定する
   /// @param randomize 抽選する場合はtrue
   void SetStartColorRandomize(bool randomize) { startColor_.randomize = randomize; }

   /// @brief 全パーティクルで共通の重力倍率を設定する
   /// @param modifier 重力加速度へ掛ける倍率
   void SetGravityModifier(float modifier) { gravityModifier_ = RandomFloat(modifier, modifier, false); }
   /// @brief 固定設定時の重力倍率を取得する
   /// @return 重力倍率範囲の下限
   float GetGravityModifier() const { return gravityModifier_.minValue; }

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
   float duration_ = 5.0f;
   bool looping_ = true;

   RandomFloat startLifetime_{ 2.0f, 2.5f, false };
   RandomFloat startSpeed_{ 5.0f, 6.0f, false };
   StartSpeedMode startSpeedMode_ = StartSpeedMode::Directional;
   RandomVector3 startVelocity_{ Vector3(0.0f, 5.0f, 0.0f), Vector3(0.0f, 6.0f, 0.0f), false };
   RandomVector3 startSize_{ Vector3(1.0f, 1.0f, 1.0f), Vector3(1.0f, 1.0f, 1.0f), false };
   RandomVector3 startRotation_{ Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), false };
   RandomColor startColor_{ 0xFFFFFFFF, 0xFFFFFFFF, false };

   RandomFloat gravityModifier_{ 0.0f, 0.0f, false };
   float timeScale_ = 1.0f;
   SimulationSpace simulationSpace_ = SimulationSpace::World;
   bool playOnAwake_ = true;
   uint32_t maxParticles_ = 1000;
};
}
