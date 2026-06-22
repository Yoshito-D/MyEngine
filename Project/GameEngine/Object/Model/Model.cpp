#include "pch.h"
#include "Model.h"
#include "ResourceHelper.h"
#include "Scene/Camera/Camera.h"
#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Component/MaterialComponent.h"
#include "Component/AnimationComponent.h"
#include "Component/ModelAssetComponent.h"
#include <algorithm>

namespace {
Logger& log_ = Logger::GetInstance();

std::string BuildDefaultModelName(const std::vector<GameEngine::Model*>& registeredModels) {
   auto exists = [&registeredModels](const std::string& name) {
	  for (const auto* model : registeredModels) {
		 if (model && model->GetObjectName() == name) {
			return true;
		 }
	  }
	  return false;
   };

   uint32_t index = 1;
   while (true) {
	  const std::string candidate = "Model_" + std::to_string(index++);
	  if (!exists(candidate)) {
		 return candidate;
	  }
   }
}
}

namespace GameEngine {

std::vector<Model*> Model::sRegisteredModels_{};

Model::Model() {
   SetObjectName(BuildDefaultModelName(sRegisteredModels_));
   sRegisteredModels_.push_back(this);
}

Model::~Model() {
   auto it = std::find(sRegisteredModels_.begin(), sRegisteredModels_.end(), this);
   if (it != sRegisteredModels_.end()) {
	  sRegisteredModels_.erase(it);
   }
}

const std::vector<Model*>& Model::GetRegisteredModels() {
   return sRegisteredModels_;
}

Model& Model::SetModelAsset(const std::shared_ptr<ModelAsset>& modelAsset) {
   if (auto* c = GetComponent<ModelAssetComponent>()) {
	  c->SetModelAsset(modelAsset);
   }
   return *this;
}

Model& Model::SetMaterial(Material* material) {
   if (material) {
	  if (auto* materialComponent = GetComponent<MaterialComponent>()) {
		 materialComponent->AssignMaterial(material);
	  }
   }
   return *this;
}

Model& Model::Create() {
   transformationMatrix_ = std::make_unique<TransformationMatrix>();
   transformationMatrix_->Create();

   AddComponent<ModelAssetComponent>();
   AddComponent<RenderComponent>();

   auto* transformComponent = GetComponent<TransformComponent>();
   if (transformComponent) {
	  transformComponent->transform.scale = Vector3(1.0f, 1.0f, 1.0f);
   }
   return *this;
}

const Vector3& Model::GetPosition() const {
   static const Vector3 zero = Vector3(0.0f, 0.0f, 0.0f);
   const auto* transformComponent = GetComponent<TransformComponent>();

   if (!transformComponent) {
	  return zero;
   }
   return transformComponent->transform.translation;
}

const Vector3& Model::GetRotation() const {
   static const Vector3 zero = Vector3(0.0f, 0.0f, 0.0f);
   const auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return zero;
   }
   return transformComponent->transform.rotation;
}

const Vector3& Model::GetScale() const {
   static const Vector3 one(1.0f, 1.0f, 1.0f);
   const auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return one;
   }
   return transformComponent->transform.scale;
}

void Model::SetTransform(const Transform& transform) {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return;
   }
   transformComponent->transform = transform;
}

void Model::SetPosition(const Vector3& translation) {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return;
   }
   transformComponent->transform.translation = translation;
}

void Model::SetRotation(const Vector3& rotation) {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return;
   }
   transformComponent->transform.SetRotationEuler(rotation);
}

void Model::SetRotationQuaternion(const Quaternion& quaternion) {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return;
   }
   transformComponent->transform.SetRotationQuaternion(quaternion);
}

const Quaternion& Model::GetRotationQuaternion() const {
   static const Quaternion identity = Quaternion::Identity();
   const auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return identity;
   }

   return transformComponent->transform.rotationQuaternion;
}

void Model::SetUseQuaternion(bool use) {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return;
   }

   auto& transform = transformComponent->transform;
   if (use) {
	  transform.SetRotationEuler(transform.rotation);
	  transform.rotationSource = Transform::RotationSource::Quaternion;
   } else {
	  transform.rotation = transform.GetActiveEuler();
	  transform.rotationSource = Transform::RotationSource::Euler;
   }
}

bool Model::IsUsingQuaternion() const {
   const auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return false;
   }

   return transformComponent->transform.IsUsingQuaternion();
}

void Model::SetScale(const Vector3& scale) {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return;
   }
   transformComponent->transform.scale = scale;
}

void Model::SetParentMatrix(const Matrix4x4& parentMatrix) {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return;
   }
   transformComponent->parentMatrix = parentMatrix;
}

void Model::UpdateMatrix(Camera* camera) {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent || !camera || !transformationMatrix_) {
	  return;
   }

   Matrix4x4 worldMatrix = MakeAffineMatrix(transformComponent->transform);

   if (hasWorldMatrixOverride_) {
	  worldMatrix = worldMatrixOverride_;
   }

   // modelAssetのrootNode.localMatrixを掛ける
   ModelAsset* modelAsset = GetComponent<ModelAssetComponent>()->GetModelAsset();
   if (modelAsset) {
	  if (!modelAsset->HasSkinningData()) {
		 worldMatrix = modelAsset->GetRootNode().localMatrix * worldMatrix;
	  }
   }

   if (transformComponent->useParentMatrix) {
	  Matrix4x4 wVPMatrix = worldMatrix * transformComponent->parentMatrix * camera->GetViewProjectionMatrix();
	  transformationMatrix_->GetTransformationMatrixData()->world = worldMatrix * transformComponent->parentMatrix;
	  transformationMatrix_->GetTransformationMatrixData()->wVP = wVPMatrix;
      transformationMatrix_->GetTransformationMatrixData()->worldInverseTranspose = (worldMatrix * transformComponent->parentMatrix).Inverse().Transpose();
   } else {
	  Matrix4x4 wVPMatrix = worldMatrix * camera->GetViewProjectionMatrix();
	  transformationMatrix_->GetTransformationMatrixData()->world = worldMatrix;
	  transformationMatrix_->GetTransformationMatrixData()->wVP = wVPMatrix;
	  transformationMatrix_->GetTransformationMatrixData()->worldInverseTranspose = worldMatrix.Inverse().Transpose();
   }
}
}
