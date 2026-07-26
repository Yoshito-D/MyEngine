#pragma once
#include "ParticleModule.h"
#include "MainModule.h"

namespace GameEngine {
/// @brief パーティクルの寿命に沿ってUVの移動・回転・拡縮を制御するモジュール
class UVTransformModule : public ParticleModule {
public:
	/// @brief UV各要素の値を決める方法
	enum class ValueMode {
		Constant = 0,                  ///< 全パーティクルで固定値を使う
		RandomBetweenTwoConstants = 1, ///< 生成時に範囲から値を選ぶ
		Curve = 2                      ///< 寿命進行度で始点と終点を補間する
	};

	/// @brief 単位スケールかつ変形なしの設定でモジュールを構築する
	UVTransformModule();

	/// @brief 設定モードに従って生成直後のUV状態を初期化する
	/// @param particle 初期化するパーティクル
	void InitializeParticle(Particle& particle) const;
	/// @brief 寿命進行度と経過時間からUV状態を更新する
	/// @param particle 更新するパーティクル
	/// @param deltaTime 前回更新からの経過時間（秒）
	void UpdateUV(Particle& particle, float deltaTime) const;

	/// @brief UVスクロール値の決定方法を設定する
	/// @param mode 使用する値モード
	void SetScrollMode(ValueMode mode) { scrollMode_ = mode; }
	/// @brief UVスクロール値の決定方法を取得する
	/// @return 現在の値モード
	ValueMode GetScrollMode() const { return scrollMode_; }
	/// @brief 固定スクロール速度を設定する
	/// @param value 1秒当たりのUV移動量
	void SetScrollConstant(const Vector2& value) { scrollConstant_ = value; }
	/// @brief 固定スクロール速度を取得する
	/// @return 1秒当たりのUV移動量
	const Vector2& GetScrollConstant() const { return scrollConstant_; }
	/// @brief 生成時に選ぶスクロール速度範囲を設定する
	/// @param value X・Y成分の乱数範囲
	void SetScrollRandom(const RandomVector2& value) { scrollRandom_ = value; }
	/// @brief スクロール速度の乱数範囲を取得する
	/// @return X・Y成分の乱数範囲
	const RandomVector2& GetScrollRandom() const { return scrollRandom_; }
	/// @brief カーブモードの開始スクロール速度を設定する
	/// @param value 寿命開始時の速度
	void SetScrollCurveStart(const Vector2& value) { scrollCurveStart_ = value; }
	/// @brief カーブモードの開始スクロール速度を取得する
	/// @return 寿命開始時の速度
	const Vector2& GetScrollCurveStart() const { return scrollCurveStart_; }
	/// @brief カーブモードの終了スクロール速度を設定する
	/// @param value 寿命終了時の速度
	void SetScrollCurveEnd(const Vector2& value) { scrollCurveEnd_ = value; }
	/// @brief カーブモードの終了スクロール速度を取得する
	/// @return 寿命終了時の速度
	const Vector2& GetScrollCurveEnd() const { return scrollCurveEnd_; }

	/// @brief UV回転値の決定方法を設定する
	/// @param mode 使用する値モード
	void SetRotationMode(ValueMode mode) { rotationMode_ = mode; }
	/// @brief UV回転値の決定方法を取得する
	/// @return 現在の値モード
	ValueMode GetRotationMode() const { return rotationMode_; }
	/// @brief 固定UV回転量を設定する
	/// @param value 回転量
	void SetRotationConstant(float value) { rotationConstant_ = value; }
	/// @brief 固定UV回転量を取得する
	/// @return 回転量
	float GetRotationConstant() const { return rotationConstant_; }
	/// @brief 生成時に選ぶUV回転範囲を設定する
	/// @param value 回転量の乱数範囲
	void SetRotationRandom(const RandomFloat& value) { rotationRandom_ = value; }
	/// @brief UV回転量の乱数範囲を取得する
	/// @return 回転量の乱数範囲
	const RandomFloat& GetRotationRandom() const { return rotationRandom_; }
	/// @brief カーブモードの開始UV回転量を設定する
	/// @param value 寿命開始時の回転量
	void SetRotationCurveStart(float value) { rotationCurveStart_ = value; }
	/// @brief カーブモードの開始UV回転量を取得する
	/// @return 寿命開始時の回転量
	float GetRotationCurveStart() const { return rotationCurveStart_; }
	/// @brief カーブモードの終了UV回転量を設定する
	/// @param value 寿命終了時の回転量
	void SetRotationCurveEnd(float value) { rotationCurveEnd_ = value; }
	/// @brief カーブモードの終了UV回転量を取得する
	/// @return 寿命終了時の回転量
	float GetRotationCurveEnd() const { return rotationCurveEnd_; }

	/// @brief UVスケール値の決定方法を設定する
	/// @param mode 使用する値モード
	void SetScaleMode(ValueMode mode) { scaleMode_ = mode; }
	/// @brief UVスケール値の決定方法を取得する
	/// @return 現在の値モード
	ValueMode GetScaleMode() const { return scaleMode_; }
	/// @brief 固定UVスケールを設定する
	/// @param value UVの拡縮率
	void SetScaleConstant(const Vector2& value) { scaleConstant_ = value; }
	/// @brief 固定UVスケールを取得する
	/// @return UVの拡縮率
	const Vector2& GetScaleConstant() const { return scaleConstant_; }
	/// @brief 生成時に選ぶUVスケール範囲を設定する
	/// @param value X・Y拡縮率の乱数範囲
	void SetScaleRandom(const RandomVector2& value) { scaleRandom_ = value; }
	/// @brief UVスケールの乱数範囲を取得する
	/// @return X・Y拡縮率の乱数範囲
	const RandomVector2& GetScaleRandom() const { return scaleRandom_; }
	/// @brief カーブモードの開始UVスケールを設定する
	/// @param value 寿命開始時の拡縮率
	void SetScaleCurveStart(const Vector2& value) { scaleCurveStart_ = value; }
	/// @brief カーブモードの開始UVスケールを取得する
	/// @return 寿命開始時の拡縮率
	const Vector2& GetScaleCurveStart() const { return scaleCurveStart_; }
	/// @brief カーブモードの終了UVスケールを設定する
	/// @param value 寿命終了時の拡縮率
	void SetScaleCurveEnd(const Vector2& value) { scaleCurveEnd_ = value; }
	/// @brief カーブモードの終了UVスケールを取得する
	/// @return 寿命終了時の拡縮率
	const Vector2& GetScaleCurveEnd() const { return scaleCurveEnd_; }

	/// @copydoc ParticleModule::ToJson
	nlohmann::json ToJson() const override;
	/// @copydoc ParticleModule::FromJson
	void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI
	/// @copydoc ParticleModule::DrawInspector
	void DrawInspector() override;
#endif

private:
	static Vector2 LerpVector2(const Vector2& a, const Vector2& b, float t);
	static float LerpFloat(float a, float b, float t);

	ValueMode scrollMode_ = ValueMode::Constant;
	ValueMode rotationMode_ = ValueMode::Constant;
	ValueMode scaleMode_ = ValueMode::Constant;

	Vector2 scrollConstant_{0.0f, 0.0f};
	RandomVector2 scrollRandom_{Vector2{0.0f, 0.0f}, Vector2{0.0f, 0.0f}, false};
	Vector2 scrollCurveStart_{0.0f, 0.0f};
	Vector2 scrollCurveEnd_{0.0f, 0.0f};

	float rotationConstant_ = 0.0f;
	RandomFloat rotationRandom_{0.0f, 0.0f, false};
	float rotationCurveStart_ = 0.0f;
	float rotationCurveEnd_ = 0.0f;

	Vector2 scaleConstant_{1.0f, 1.0f};
	RandomVector2 scaleRandom_{Vector2{1.0f, 1.0f}, Vector2{1.0f, 1.0f}, false};
	Vector2 scaleCurveStart_{1.0f, 1.0f};
	Vector2 scaleCurveEnd_{1.0f, 1.0f};
};
}
