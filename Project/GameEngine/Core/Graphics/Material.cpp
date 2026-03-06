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

   // 平行移動を取得
   outTranslation = Vector2(mat.m[3][0], mat.m[3][1]);

   // スケールを計算
   outScale.x = Vector2(mat.m[0][0], mat.m[0][1]).Length();
   outScale.y = Vector2(mat.m[1][0], mat.m[1][1]).Length();

   // 回転を計算（スケールで正規化後）
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

   // 正しい変換順序：Scale → Rotate → Translate
   materialData_->uvTransform = scaleMat * rotMat * transMat;
}
}
