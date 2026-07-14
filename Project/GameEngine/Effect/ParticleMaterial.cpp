#include "pch.h"
#include "ParticleMaterial.h"
#include "GraphicsDevice.h"
#include "ResourceHelper.h"
#include <algorithm>

namespace GameEngine {
ParticleMaterial::ParticleMaterial() {}

void ParticleMaterial::Create(GraphicsDevice* device, const Vector4& color) {
   // マテリアル用バッファを作成
   materialResource_ = ResourceHelper::CreateBufferResource(
	  device->GetDevice(),
	  sizeof(MaterialData)
   );

   // マッピング
   materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

   // 初期値設定
   materialData_->color = color;
   materialData_->uvTransform = MakeIdentity4x4();
   materialData_->renderingParams = Vector4(brightness_, alphaCutoff_, static_cast<float>(toonSteps_), 0.0f);
   materialData_->effectParams = Vector4(
	  softParticlesEnabled_ ? 1.0f : 0.0f,
	  softParticleDistance_,
	  distortionStrength_,
	  distortionBlend_);
   materialData_->sceneParams = Vector4(1.0f, 1.0f, 0.01f, 100.0f);
   materialData_->projectionParams = Vector4(
	  0.0f,
	  distortionUseTextureFlow_ ? 1.0f : 0.0f,
	  0.0f,
	  0.0f);
}

void ParticleMaterial::SetColor(const Vector4& color) {
   if (materialData_) {
	  materialData_->color = color;
   }
}

void ParticleMaterial::SetUVTransform(const Matrix4x4& transform) {
   if (materialData_) {
	  materialData_->uvTransform = transform;
   }
}

void ParticleMaterial::SetBrightness(float brightness) {
   brightness_ = (std::max)(brightness, 0.0f);
   if (materialData_) {
	  materialData_->renderingParams.x = brightness_;
   }
}

void ParticleMaterial::SetAlphaCutoff(float cutoff) {
   alphaCutoff_ = std::clamp(cutoff, 0.0f, 1.0f);
   if (materialData_) {
	  materialData_->renderingParams.y = alphaCutoff_;
   }
}

void ParticleMaterial::SetToonSteps(uint32_t steps) {
   toonSteps_ = steps;
   if (materialData_) {
	  materialData_->renderingParams.z = static_cast<float>(toonSteps_);
   }
}

float ParticleMaterial::GetBrightness() const {
   return brightness_;
}

float ParticleMaterial::GetAlphaCutoff() const {
   return alphaCutoff_;
}

uint32_t ParticleMaterial::GetToonSteps() const {
   return toonSteps_;
}

void ParticleMaterial::SetSoftParticlesEnabled(bool enabled) {
   softParticlesEnabled_ = enabled;
   if (materialData_) {
	  materialData_->effectParams.x = enabled ? 1.0f : 0.0f;
   }
}

void ParticleMaterial::SetSoftParticleDistance(float distance) {
   softParticleDistance_ = (std::max)(distance, 0.0001f);
   if (materialData_) {
	  materialData_->effectParams.y = softParticleDistance_;
   }
}

void ParticleMaterial::SetDistortionStrength(float strength) {
   distortionStrength_ = strength;
   if (materialData_) {
	  materialData_->effectParams.z = distortionStrength_;
   }
}

void ParticleMaterial::SetDistortionBlend(float blend) {
   distortionBlend_ = std::clamp(blend, 0.0f, 1.0f);
   if (materialData_) {
	  materialData_->effectParams.w = distortionBlend_;
   }
}

void ParticleMaterial::SetDistortionUseTextureFlow(bool enabled) {
   distortionUseTextureFlow_ = enabled;
   if (materialData_) {
	  materialData_->projectionParams.y = enabled ? 1.0f : 0.0f;
   }
}

void ParticleMaterial::SetSceneParameters(float width, float height, float nearClip, float farClip, bool orthographic) {
   if (!materialData_) {
	  return;
   }
   materialData_->sceneParams = Vector4(
	  (std::max)(width, 1.0f),
	  (std::max)(height, 1.0f),
	  (std::max)(nearClip, 0.0001f),
	  (std::max)(farClip, nearClip + 0.0001f));
   materialData_->projectionParams.x = orthographic ? 1.0f : 0.0f;
}
}
