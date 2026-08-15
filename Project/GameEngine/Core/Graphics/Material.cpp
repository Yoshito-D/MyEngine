#include "pch.h"
#include "Material.h"
#include "ResourceHelper.h"
#include "GraphicsDevice.h"

namespace GameEngine {
namespace {
GraphicsDevice* sDevice_ = nullptr;
bool sIsInitialized_ = false;
}

void Material::Initialize(GraphicsDevice* device) {
   if (sIsInitialized_) return;
   sDevice_ = device;
   sIsInitialized_ = true;
}

void Material::Create(unsigned int color, int32_t lightingMode, const Matrix4x4& uvTransform, float shininess) {
   if (!sIsInitialized_)return;
   // シェーダーからCBVとして直接読むUPLOADバッファを永続Mapし、
   // セッターの変更を次回描画へ即時反映できるようにする。
   materialResource_ = ResourceHelper::CreateBufferResource(sDevice_->GetDevice(), sizeof(MaterialData));
   // 書き込むためのアドレスを取得
   materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
   // 色
   materialData_->color = ConvertUIntToColor(color);
   // ライティングするか
   materialData_->lightingMode = lightingMode;
   // UV
   materialData_->uvTransform = uvTransform;
   // 光沢
   materialData_->shininess = shininess;
   // 環境マップ強度
   materialData_->environmentCoefficient = 0.0f;
   // 既存マテリアルの見た目を維持し、必要なオブジェクトだけ補助光を有効にする。
   materialData_->rimLightColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
   materialData_->rimLightIntensity = 0.0f;
   materialData_->rimLightPower = 4.0f;
   materialData_->fillLightColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
   materialData_->fillLightIntensity = 0.0f;
}

// ========== プロパティアクセス関数の実装 ==========

void Material::SetColor(const Vector4& color) {
   if (materialData_) {
	  materialData_->color = color;
   }
}

void Material::SetColor(unsigned int color) {
   if (materialData_) {
	  materialData_->color = ConvertUIntToColor(color);
   }
}

Vector4 Material::GetColor() const {
   if (materialData_) {
	  return materialData_->color;
   }
   return Vector4(1.0f, 1.0f, 1.0f, 1.0f);
}

void Material::SetLightingMode(LightingMode mode) {
   if (materialData_) {
	  materialData_->lightingMode = static_cast<int32_t>(mode);
   }
}

Material::LightingMode Material::GetLightingMode() const {
   if (materialData_) {
	  return static_cast<LightingMode>(materialData_->lightingMode);
   }
   return LightingMode::HALFLAMBERT;
}

void Material::SetShininess(float shininess) {
   if (materialData_) {
	  materialData_->shininess = shininess;
   }
}

float Material::GetShininess() const {
   if (materialData_) {
	  return materialData_->shininess;
   }
   return 40.0f;
}

void Material::SetEnvironmentTextureStrength(float strength) {
   if (materialData_) {
	  materialData_->environmentCoefficient = strength;
   }
}

float Material::GetEnvironmentTextureStrength() const {
   if (materialData_) {
	  return materialData_->environmentCoefficient;
   }
   return 0.0f;
}

void Material::SetRimLightColor(const Vector4& color) {
   if (materialData_) {
	  materialData_->rimLightColor = color;
   }
}

Vector4 Material::GetRimLightColor() const {
   if (materialData_) {
	  return materialData_->rimLightColor;
   }
   return Vector4(1.0f, 1.0f, 1.0f, 1.0f);
}

void Material::SetRimLightIntensity(float intensity) {
   if (materialData_) {
	  materialData_->rimLightIntensity = std::max(intensity, 0.0f);
   }
}

float Material::GetRimLightIntensity() const {
   if (materialData_) {
	  return materialData_->rimLightIntensity;
   }
   return 0.0f;
}

void Material::SetRimLightPower(float power) {
   if (materialData_) {
	  materialData_->rimLightPower = std::max(power, 0.01f);
   }
}

float Material::GetRimLightPower() const {
   if (materialData_) {
	  return materialData_->rimLightPower;
   }
   return 4.0f;
}

void Material::SetFillLightColor(const Vector4& color) {
   if (materialData_) {
	  materialData_->fillLightColor = color;
   }
}

Vector4 Material::GetFillLightColor() const {
   if (materialData_) {
	  return materialData_->fillLightColor;
   }
   return Vector4(1.0f, 1.0f, 1.0f, 1.0f);
}

void Material::SetFillLightIntensity(float intensity) {
   if (materialData_) {
	  materialData_->fillLightIntensity = std::max(intensity, 0.0f);
   }
}

float Material::GetFillLightIntensity() const {
   if (materialData_) {
	  return materialData_->fillLightIntensity;
   }
   return 0.0f;
}

// ========== UVTransform操作関数の実装 ==========

void Material::SetUVTransform(const Matrix4x4& transform) {
   if (materialData_) {
	  materialData_->uvTransform = transform;
   }
}

Matrix4x4 Material::GetUVTransform() const {
   if (materialData_) {
	  return materialData_->uvTransform;
   }
   return MakeIdentity4x4();
}

void Material::SetUVTransform(const Vector2& scale, float rotation, const Vector2& translation) {
   ComposeUVTransform(scale, rotation, translation);
}

void Material::SetUVScale(const Vector2& scale) {
   Vector2 currentScale, currentTranslation;
   float currentRotation;
   DecomposeUVTransform(currentScale, currentRotation, currentTranslation);
   ComposeUVTransform(scale, currentRotation, currentTranslation);
}

void Material::SetUVRotation(float rotation) {
   Vector2 currentScale, currentTranslation;
   float currentRotation;
   DecomposeUVTransform(currentScale, currentRotation, currentTranslation);
   ComposeUVTransform(currentScale, rotation, currentTranslation);
}

void Material::SetUVTranslation(const Vector2& translation) {
   Vector2 currentScale, currentTranslation;
   float currentRotation;
   DecomposeUVTransform(currentScale, currentRotation, currentTranslation);
   ComposeUVTransform(currentScale, currentRotation, translation);
}

Vector2 Material::GetUVScale() const {
   Vector2 scale, translation;
   float rotation;
   DecomposeUVTransform(scale, rotation, translation);
   return scale;
}

float Material::GetUVRotation() const {
   Vector2 scale, translation;
   float rotation;
   DecomposeUVTransform(scale, rotation, translation);
   return rotation;
}

Vector2 Material::GetUVTranslation() const {
   Vector2 scale, translation;
   float rotation;
   DecomposeUVTransform(scale, rotation, translation);
   return translation;
}

void Material::ResetUVTransform() {
   if (materialData_) {
	  materialData_->uvTransform = MakeIdentity4x4();
   }
}

// ========== プライベート関数の実装 ==========

void Material::DecomposeUVTransform(Vector2& outScale, float& outRotation, Vector2& outTranslation) const {
   if (!materialData_) {
	  outScale = Vector2(1.0f, 1.0f);
	  outRotation = 0.0f;
	  outTranslation = Vector2(0.0f, 0.0f);
	  return;
   }

   const Matrix4x4& mat = materialData_->uvTransform;

   // ComposeUVTransformと同じ行ベクトル規約を前提に、最終行から平行移動を復元する。
   outTranslation = Vector2(mat.m[3][0], mat.m[3][1]);

   // UV行列にせん断が含まれない前提で、回転を含む各基底行の長さをスケールとみなす。
   // 符号付きスケールは保持できないため、この分解はUI編集用のSRT行列に限定される。
   outScale.x = Vector2(mat.m[0][0], mat.m[0][1]).Length();
   outScale.y = Vector2(mat.m[1][0], mat.m[1][1]).Length();

   // X基底をスケールで正規化して回転成分だけを取り出す。
   if (outScale.x > 0.0f) {
	  float cosTheta = mat.m[0][0] / outScale.x;
	  float sinTheta = mat.m[0][1] / outScale.x; // X軸の成分を使ってsinThetaを計算
	  outRotation = std::atan2(sinTheta, cosTheta);
   } else {
	  outRotation = 0.0f;
   }
}

void Material::ComposeUVTransform(const Vector2& scale, float rotation, const Vector2& translation) {
   if (!materialData_) return;

   Matrix4x4 scaleMat = MakeScaleMatrix(Vector3(scale.x, scale.y, 1.0f));
   Matrix4x4 rotMat = MakeRotateZMatrix(rotation);
   Matrix4x4 transMat = MakeTranslateMatrix(Vector3(translation.x, translation.y, 0.0f));

   // エンジンの行ベクトル規約に合わせてScale → Rotate → Translateの順に合成する。
   // 順序を変えると平行移動まで拡縮・回転され、インスペクター値と見た目が一致しない。
   materialData_->uvTransform = scaleMat * rotMat * transMat;
}
}
