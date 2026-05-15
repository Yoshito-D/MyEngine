#include "pch.h"
#include "Skybox.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/ResourceHelper.h"
#include <algorithm>

namespace GameEngine {

std::vector<Skybox*> Skybox::sRegisteredSkyboxes_{};

Skybox::Skybox() {
   sRegisteredSkyboxes_.push_back(this);

   static uint32_t skyboxCounter = 0;
   SetObjectName("Skybox_" + std::to_string(++skyboxCounter));
}

Skybox::~Skybox() {
   auto it = std::find(sRegisteredSkyboxes_.begin(), sRegisteredSkyboxes_.end(), this);
   if (it != sRegisteredSkyboxes_.end()) {
	  sRegisteredSkyboxes_.erase(it);
   }
}

const std::vector<Skybox*>& Skybox::GetRegisteredSkyboxes() {
   return sRegisteredSkyboxes_;
}

void Skybox::Create(GraphicsDevice* device) {
   mesh_.CreateSkybox();

   transformResource_ = ResourceHelper::CreateBufferResource(device->GetDevice(), sizeof(SkyboxTransformData));
   transformResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformData_));
   transformData_->wVP = MakeIdentity4x4();
   transformData_->world = MakeIdentity4x4();
   transformData_->worldInverseTranspose = MakeIdentity4x4();

   materialResource_ = ResourceHelper::CreateBufferResource(device->GetDevice(), sizeof(SkyboxMaterialData));
   materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
   materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
}

void Skybox::SetTexture(Texture* texture) {
   texture_ = texture;
}

void Skybox::SetColor(const Vector4& color) {
   if (materialData_) {
	  materialData_->color = color;
   }
}

ID3D12Resource* Skybox::GetTransformResource() const {
   return transformResource_.Get();
}

ID3D12Resource* Skybox::GetMaterialResource() const {
   return materialResource_.Get();
}

void Skybox::UpdateTransform(const Matrix4x4& viewProjectionMatrix) {
   if (transformData_) {
	  // VSシェーダーは wVP のみ使用する
	  transformData_->wVP = viewProjectionMatrix;
	  transformData_->world = MakeIdentity4x4();
	  transformData_->worldInverseTranspose = MakeIdentity4x4();
   }
}

} // namespace GameEngine
