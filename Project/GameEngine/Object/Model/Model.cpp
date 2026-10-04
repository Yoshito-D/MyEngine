#include "GameEngine/pch.h"
#include "GameEngine/Object/Model/Model.h"
#include "GameEngine/Graphics/Device/ResourceHelper.h"
#include "GameEngine/Scene/Camera/Camera.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Component/Rendering/RenderComponent.h"
#include "GameEngine/Object/Component/Rendering/MaterialComponent.h"
#include "GameEngine/Object/Component/Animation/AnimationComponent.h"
#include "GameEngine/Object/Component/Rendering/MeshComponent.h"
#include <algorithm>

namespace {

// モデルごとに独立したマテリアルインスタンスを割り当てるためのプロセス内通番。
// アセット名ではなくインスタンス名なので、同じモデルアセットを複数配置しても編集内容が混線しない。
uint64_t sAutoModelMaterialCounter = 0;

std::string BuildAutoModelMaterialName() {
   return "ModelMaterial_" + std::to_string(++sAutoModelMaterialCounter);
}

std::string BuildDefaultModelName(const std::vector<GameEngine::Model*>& registeredModels) {
   // シーン内で参照しやすい表示名を付けるため、現在生存しているモデルとの重複だけを調べる。
   // 削除済み番号は再利用できるため、単調増加カウンターより不要な番号の肥大化を避けられる。
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
   // Model が描画可能であるという前提をどの生成経路でも満たすよう、
   // 変換・マテリアル・メッシュ・描画の必須コンポーネントをコンストラクタで揃える。
   auto* transformComponent = AddComponent<TransformComponent>();
   {
      auto pose = transformComponent->GetLocalPose();
      pose.scale = Vector3(1.0f, 1.0f, 1.0f);
      transformComponent->ApplyLocalPose(pose);
   }
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

   // 登録はコンストラクタで一度だけ行うため、最初の一致を消せば一覧の整合性を回復できる。
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
	  {
	     auto pose = transformComponent->GetLocalPose();
	     pose.scale = Vector3(1.0f, 1.0f, 1.0f);
	     transformComponent->ApplyLocalPose(pose);
	  }
	  transformComponent->EnsureTransformationMatrix();
   }
   return *this;
}

const Vector3& Model::GetPosition() const {
   // 参照を返す API のため、コンポーネント欠落時も寿命が切れない静的既定値を返す。
   static const Vector3 zero = Vector3(0.0f, 0.0f, 0.0f);
   const auto* transformComponent = GetComponent<TransformComponent>();

   if (!transformComponent) {
	  return zero;
   }
   return transformComponent->GetLocalPose().translation;
}

Vector3 Model::GetRotation() const {
   static const Vector3 zero = Vector3(0.0f, 0.0f, 0.0f);
   const auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return zero;
   }
   return transformComponent->GetLocalPose().GetActiveEuler();
}

const Vector3& Model::GetScale() const {
   static const Vector3 one(1.0f, 1.0f, 1.0f);
   const auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return one;
   }
   return transformComponent->GetLocalPose().scale;
}

void Model::SetTransform(const Transform& transform) {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return;
   }
   transformComponent->ApplyLocalPose(transform);
}

void Model::SetPosition(const Vector3& translation) {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return;
   }
   {
      auto pose = transformComponent->GetLocalPose();
      pose.translation = translation;
      transformComponent->ApplyLocalPose(pose);
   }
}

void Model::SetRotation(const Vector3& rotation) {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return;
   }
   {
      auto pose = transformComponent->GetLocalPose();
      pose.SetRotationEuler(rotation);
      transformComponent->ApplyLocalPose(pose);
   }
}

void Model::SetRotationQuaternion(const Quaternion& quaternion) {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return;
   }
   {
      auto pose = transformComponent->GetLocalPose();
      pose.SetRotationQuaternion(quaternion);
      transformComponent->ApplyLocalPose(pose);
   }
}

Quaternion Model::GetRotationQuaternion() const {
   static const Quaternion identity = Quaternion::Identity();
   const auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return identity;
   }

   return transformComponent->GetLocalPose().GetActiveQuaternion();
}

void Model::SetUseQuaternion(bool use) {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return;
   }

   auto transform = transformComponent->GetLocalPose();
   if (use) transform.SetRotationQuaternion(transform.GetActiveQuaternion());
   else transform.SetRotationEuler(transform.GetActiveEuler());
   transformComponent->ApplyLocalPose(transform);
}

bool Model::IsUsingQuaternion() const {
   const auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return false;
   }

   return transformComponent->GetLocalPose().IsUsingQuaternion();
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
   {
      auto pose = transformComponent->GetLocalPose();
      pose.scale = scale;
      transformComponent->ApplyLocalPose(pose);
   }
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

   // 通常はローカル Transform から組み立てるが、物理演算などが最終行列を供給した場合は
   // その結果を優先し、二重にスケール・回転・平行移動を適用しない。
   Matrix4x4 worldMatrix = MakeAffineMatrix(transformComponent->GetLocalPose());

   if (transformComponent->HasWorldMatrixOverride()) {
	  worldMatrix = transformComponent->GetWorldMatrixOverride();
   }

   // modelAssetのrootNode.localMatrixを掛ける
   const ModelAsset* modelAsset = GetComponent<MeshComponent>()->GetModelAsset();
   if (modelAsset) {
      // スキニングモデルではボーン階層がルート変換を担うため、ここで再適用すると二重変換になる。
	  if (!modelAsset->HasSkinningData()) {
		 worldMatrix = modelAsset->GetRootNode().localMatrix * worldMatrix;
	  }
   }

   worldMatrix = transformComponent->ComposeWorldMatrix(worldMatrix);
   transformationMatrix->ApplyWorldTransform(worldMatrix, camera->GetViewProjectionMatrix());
}

TransformationMatrix* Model::GetTransformationMatrix() {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return nullptr;
   }
   return transformComponent->EnsureTransformationMatrix();
}
}
