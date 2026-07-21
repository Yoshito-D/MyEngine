#include "pch.h"
#include "Model.h"
#include "ResourceHelper.h"
#include "Scene/Camera/Camera.h"
#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Component/MaterialComponent.h"
#include "Component/Model/AnimationComponent.h"
#include "Component/MeshComponent.h"
#include <algorithm>

namespace {

uint64_t sAutoModelMaterialCounter = 0;

std::string BuildAutoModelMaterialName() {
   return "ModelMaterial_" + std::to_string(++sAutoModelMaterialCounter);
}

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
   auto* transformComponent = AddComponent<TransformComponent>();
   transformComponent->transform.scale = Vector3(1.0f, 1.0f, 1.0f);
   if (auto* materialComponent = AddComponent<MaterialComponent>()) {
      materialComponent->EnsureMaterial(BuildAutoModelMaterialName());
   }
   AddComponent<MeshComponent>();
   AddComponent<RenderComponent>();
   SetObjectName(BuildDefaultModelName(sRegisteredModels_));
   sRegisteredModels_.push_back(this);
}

Model::~Model() {
   UnregisterModel(this);
}

void Model::UnregisterModel(Model* model) {
   if (!model) {
	  return;
   }

   auto it = std::find(sRegisteredModels_.begin(), sRegisteredModels_.end(), model);
   if (it != sRegisteredModels_.end()) {
	  sRegisteredModels_.erase(it);
   }
}

const std::vector<Model*>& Model::GetRegisteredModels() {
   return sRegisteredModels_;
}

Model& Model::SetModelAsset(const std::shared_ptr<ModelAsset>& modelAsset) {
   if (auto* c = GetComponent<MeshComponent>()) {
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
   AddComponent<MeshComponent>();
   AddComponent<RenderComponent>();

   auto* transformComponent = GetComponent<TransformComponent>();
   if (transformComponent) {
	  transformComponent->transform.scale = Vector3(1.0f, 1.0f, 1.0f);
	  transformComponent->EnsureTransformationMatrix();
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

void Model::SetWorldMatrix(const Matrix4x4& worldMatrix) {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return;
   }
   transformComponent->SetWorldMatrixOverride(worldMatrix);
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
   if (!transformComponent || !camera) {
	  return;
   }

   auto* transformationMatrix = transformComponent->EnsureTransformationMatrix();
   if (!transformationMatrix) {
	  return;
   }

   Matrix4x4 worldMatrix = MakeAffineMatrix(transformComponent->transform);

   if (transformComponent->HasWorldMatrixOverride()) {
	  worldMatrix = transformComponent->GetWorldMatrixOverride();
   }

   // modelAssetのrootNode.localMatrixを掛ける
   ModelAsset* modelAsset = GetComponent<MeshComponent>()->GetModelAsset();
   if (modelAsset) {
	  if (!modelAsset->HasSkinningData()) {
		 worldMatrix = modelAsset->GetRootNode().localMatrix * worldMatrix;
	  }
   }

   if (transformComponent->useParentMatrix) {
	  Matrix4x4 wVPMatrix = worldMatrix * transformComponent->parentMatrix * camera->GetViewProjectionMatrix();
	  transformationMatrix->GetTransformationMatrixData()->world = worldMatrix * transformComponent->parentMatrix;
	  transformationMatrix->GetTransformationMatrixData()->wVP = wVPMatrix;
      transformationMatrix->GetTransformationMatrixData()->worldInverseTranspose = (worldMatrix * transformComponent->parentMatrix).Inverse().Transpose();
   } else {
	  Matrix4x4 wVPMatrix = worldMatrix * camera->GetViewProjectionMatrix();
	  transformationMatrix->GetTransformationMatrixData()->world = worldMatrix;
	  transformationMatrix->GetTransformationMatrixData()->wVP = wVPMatrix;
	  transformationMatrix->GetTransformationMatrixData()->worldInverseTranspose = worldMatrix.Inverse().Transpose();
   }
}

TransformationMatrix* Model::GetTransformationMatrix() {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return nullptr;
   }
   return transformComponent->EnsureTransformationMatrix();
}
}
