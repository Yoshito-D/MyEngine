#pragma once
#include "ParticleModule.h"
#include "MainModule.h"

namespace GameEngine {
class UVTransformModule : public ParticleModule {
public:
	enum class ValueMode {
		Constant = 0,
		RandomBetweenTwoConstants = 1,
		Curve = 2
	};

	UVTransformModule();

	void InitializeParticle(Particle& particle) const;
	void UpdateUV(Particle& particle, float deltaTime) const;

	void SetScrollMode(ValueMode mode) { scrollMode_ = mode; }
	ValueMode GetScrollMode() const { return scrollMode_; }
	void SetScrollConstant(const Vector2& value) { scrollConstant_ = value; }
	const Vector2& GetScrollConstant() const { return scrollConstant_; }
	void SetScrollRandom(const RandomVector2& value) { scrollRandom_ = value; }
	const RandomVector2& GetScrollRandom() const { return scrollRandom_; }
	void SetScrollCurveStart(const Vector2& value) { scrollCurveStart_ = value; }
	const Vector2& GetScrollCurveStart() const { return scrollCurveStart_; }
	void SetScrollCurveEnd(const Vector2& value) { scrollCurveEnd_ = value; }
	const Vector2& GetScrollCurveEnd() const { return scrollCurveEnd_; }

	void SetRotationMode(ValueMode mode) { rotationMode_ = mode; }
	ValueMode GetRotationMode() const { return rotationMode_; }
	void SetRotationConstant(float value) { rotationConstant_ = value; }
	float GetRotationConstant() const { return rotationConstant_; }
	void SetRotationRandom(const RandomFloat& value) { rotationRandom_ = value; }
	const RandomFloat& GetRotationRandom() const { return rotationRandom_; }
	void SetRotationCurveStart(float value) { rotationCurveStart_ = value; }
	float GetRotationCurveStart() const { return rotationCurveStart_; }
	void SetRotationCurveEnd(float value) { rotationCurveEnd_ = value; }
	float GetRotationCurveEnd() const { return rotationCurveEnd_; }

	void SetScaleMode(ValueMode mode) { scaleMode_ = mode; }
	ValueMode GetScaleMode() const { return scaleMode_; }
	void SetScaleConstant(const Vector2& value) { scaleConstant_ = value; }
	const Vector2& GetScaleConstant() const { return scaleConstant_; }
	void SetScaleRandom(const RandomVector2& value) { scaleRandom_ = value; }
	const RandomVector2& GetScaleRandom() const { return scaleRandom_; }
	void SetScaleCurveStart(const Vector2& value) { scaleCurveStart_ = value; }
	const Vector2& GetScaleCurveStart() const { return scaleCurveStart_; }
	void SetScaleCurveEnd(const Vector2& value) { scaleCurveEnd_ = value; }
	const Vector2& GetScaleCurveEnd() const { return scaleCurveEnd_; }

	nlohmann::json ToJson() const override;
	void FromJson(const nlohmann::json& json) override;

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
