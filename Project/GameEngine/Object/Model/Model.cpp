#include "pch.h"
#include "Model.h"
#include "ResourceHelper.h"
#include "Scene/Camera/Camera.h"
#include <algorithm>

namespace {
Logger& log_ = Logger::GetInstance();
}

namespace GameEngine {

std::vector<Model*> Model::sRegisteredModels_{};

Model::Model() {
   sRegisteredModels_.push_back(this);

   static uint32_t modelCounter = 0;
   SetObjectName("Model_" + std::to_string(++modelCounter));
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

void Model::Create(ModelAsset* modelAsset, Material* material) {
   if (modelAsset) {
	  modelAsset_ = modelAsset;
   }

   CreateTransformationMatrix();

   if (material) {
     SetMaterial(material);
   }

   AddComponent<RenderComponent>();

   auto* transformComponent = GetTransformComponent();
   if (transformComponent) {
	  transformComponent->transform.scale = Vector3(1.0f, 1.0f, 1.0f);
   }
}

const Vector3& Model::GetPosition() const {
   static const Vector3 zero = Vector3(0.0f, 0.0f, 0.0f);
   const auto* transformComponent = GetTransformComponent();
   if (!transformComponent) {
	  return zero;
   }
   return transformComponent->transform.translation;
}

const Vector3& Model::GetRotation() const {
   static const Vector3 zero = Vector3(0.0f, 0.0f, 0.0f);
   const auto* transformComponent = GetTransformComponent();
   if (!transformComponent) {
	  return zero;
   }
   return transformComponent->transform.rotation;
}

const Vector3& Model::GetScale() const {
   static const Vector3 one(1.0f, 1.0f, 1.0f);
   const auto* transformComponent = GetTransformComponent();
   if (!transformComponent) {
	  return one;
   }
   return transformComponent->transform.scale;
}

void Model::SetTransform(const Transform& transform) {
   auto* transformComponent = GetTransformComponent();
   if (!transformComponent) {
	  return;
   }
   transformComponent->transform = transform;
}

void Model::SetPosition(const Vector3& translation) {
   auto* transformComponent = GetTransformComponent();
   if (!transformComponent) {
	  return;
   }
   transformComponent->transform.translation = translation;
}

void Model::SetRotation(const Vector3& rotation) {
   auto* transformComponent = GetTransformComponent();
   if (!transformComponent) {
	  return;
   }
   transformComponent->transform.SetRotationEuler(rotation);
}

void Model::SetRotationQuaternion(const Quaternion& quaternion) {
   auto* transformComponent = GetTransformComponent();
   if (!transformComponent) {
	  return;
   }
   transformComponent->transform.SetRotationQuaternion(quaternion);
}

const Quaternion& Model::GetRotationQuaternion() const {
   static const Quaternion identity = Quaternion::Identity();
   const auto* transformComponent = GetTransformComponent();
   if (!transformComponent) {
	  return identity;
   }

   return transformComponent->transform.rotationQuaternion;
}

void Model::SetUseQuaternion(bool use) {
   auto* transformComponent = GetTransformComponent();
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
   const auto* transformComponent = GetTransformComponent();
   if (!transformComponent) {
	  return false;
   }

   return transformComponent->transform.IsUsingQuaternion();
}

void Model::SetScale(const Vector3& scale) {
   auto* transformComponent = GetTransformComponent();
   if (!transformComponent) {
	  return;
   }
   transformComponent->transform.scale = scale;
}

void Model::SetParentMatrix(const Matrix4x4& parentMatrix) {
   auto* transformComponent = GetTransformComponent();
   if (!transformComponent) {
	  return;
   }
   transformComponent->parentMatrix = parentMatrix;
}

void Model::UpdateMatrix(Camera* camera) {
   auto* transformComponent = GetTransformComponent();
   if (!transformComponent || !camera || !transformationMatrix_) {
	  return;
   }

   Matrix4x4 worldMatrix = MakeAffineMatrix(transformComponent->transform);

   if (hasWorldMatrixOverride_) {
	  worldMatrix = worldMatrixOverride_;
   }

   // modelAssetのrootNode.localMatrixを掛ける
   if (modelAsset_) {
	  worldMatrix = modelAsset_->GetRootNode().localMatrix * worldMatrix;
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