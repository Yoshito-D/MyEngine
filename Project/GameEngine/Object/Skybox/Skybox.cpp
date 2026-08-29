#include "pch.h"
#include "Skybox.h"
#include "Component/Skybox/SkyboxComponent.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/ResourceHelper.h"
#include <algorithm>

namespace GameEngine {

std::vector<Skybox*> Skybox::sRegisteredSkyboxes_{};

namespace {
std::string BuildDefaultSkyboxName(const std::vector<Skybox*>& registeredSkyboxes) {
   auto exists = [&registeredSkyboxes](const std::string& name) {
	  for (const auto* skybox : registeredSkyboxes) {
		 if (skybox && skybox->GetObjectName() == name) {
			return true;
		 }
	  }
	  return false;
   };

   uint32_t index = 1;
   while (true) {
	  const std::string candidate = "Skybox_" + std::to_string(index++);
	  if (!exists(candidate)) {
		 return candidate;
	  }
   }
}
}

Skybox::Skybox() {
   AddComponent<SkyboxComponent>();
   SetObjectName(BuildDefaultSkyboxName(sRegisteredSkyboxes_));
   sRegisteredSkyboxes_.push_back(this);
}

Skybox::~Skybox() {
   UnregisterSkybox(this);
}

void Skybox::UnregisterSkybox(Skybox* skybox) {
   auto it = std::find(sRegisteredSkyboxes_.begin(), sRegisteredSkyboxes_.end(), skybox);
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
   const auto* skyboxComponent = GetComponent<SkyboxComponent>();
   materialData_->color = skyboxComponent ? skyboxComponent->GetColor() : Vector4(1.0f, 1.0f, 1.0f, 1.0f);
}

void Skybox::SetTexture(Texture* texture) {
   if (auto* skyboxComponent = GetComponent<SkyboxComponent>()) {
      skyboxComponent->SetTexture(texture);
   }
}

Texture* Skybox::GetTexture() const {
   const auto* skyboxComponent = GetComponent<SkyboxComponent>();
   return skyboxComponent ? skyboxComponent->GetTexture() : nullptr;
}

void Skybox::SetColor(const Vector4& color) {
   if (auto* skyboxComponent = GetComponent<SkyboxComponent>()) {
      skyboxComponent->SetColor(color);
   }
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
   if (materialData_) {
      if (const auto* skyboxComponent = GetComponent<SkyboxComponent>()) {
         materialData_->color = skyboxComponent->GetColor();
      }
   }
   if (transformData_) {
	  // VSシェーダーは wVP のみ使用する
	  transformData_->wVP = viewProjectionMatrix;
	  transformData_->world = MakeIdentity4x4();
	  transformData_->worldInverseTranspose = MakeIdentity4x4();
   }
}

} // namespace GameEngine
