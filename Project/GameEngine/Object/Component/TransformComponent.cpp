#include "pch.h"
#include "TransformComponent.h"
#include "ComponentRegistry.h"
#include "Object.h"

namespace {
const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
   GameEngine::TransformComponent::kTypeName,
   [](GameEngine::Object& o) -> GameEngine::IObjectComponent* { return o.AddComponent<GameEngine::TransformComponent>(); },
   GameEngine::TransformComponent::kDisplayName,
   GameEngine::ObjectType::Generic | GameEngine::ObjectType::Model | GameEngine::ObjectType::Sprite | GameEngine::ObjectType::UIText
);
}

#ifdef USE_IMGUI
#include "Scene/BaseScene.h"
#include "Utility/ImGuiHelper.h"
#include <imgui.h>
#endif

namespace GameEngine {

const char* TransformComponent::GetTypeName() const {
   return "TransformComponent";
}

TransformationMatrix* TransformComponent::EnsureTransformationMatrix() {
   // GPU定数バッファは描画対象になったComponentだけ遅延生成し、純粋な親ノードでは確保しない。
   if (!transformationMatrix_) {
	  transformationMatrix_ = std::make_unique<TransformationMatrix>();
	  transformationMatrix_->Create();
   }

   return transformationMatrix_.get();
}

void TransformComponent::SetWorldMatrixOverride(const Matrix4x4& worldMatrix) {
   // Animation等が計算済みWorld行列を供給する場合に、通常のTRS/親子合成を一時的に迂回する。
   worldMatrixOverride_ = worldMatrix;
   hasWorldMatrixOverride_ = true;
}

void TransformComponent::ClearWorldMatrixOverride() {
   worldMatrixOverride_ = MakeIdentity4x4();
   hasWorldMatrixOverride_ = false;
}

nlohmann::json TransformComponent::Serialize() const {
   // 編集に使うEuler角と補間に使うQuaternionを両方残し、rotationSourceで正本を明示する。
   // 読み書きのたびに相互変換して姿勢が少しずつずれることを防ぐ。
   const Vector3 activeEuler = transform.GetActiveEuler();
   const Quaternion activeQuaternion = transform.GetActiveQuaternion();

   return nlohmann::json{
	  { "translation", { transform.translation.x, transform.translation.y, transform.translation.z } },
	  { "rotation", { activeEuler.x, activeEuler.y, activeEuler.z } },
	  { "rotationQuaternion", { activeQuaternion.x, activeQuaternion.y, activeQuaternion.z, activeQuaternion.w } },
	  { "rotationSource", transform.IsUsingQuaternion() ? "quaternion" : "euler" },
	  { "scale", { transform.scale.x, transform.scale.y, transform.scale.z } },
	  { "parentEntityId", HasOwner() ? GetOwner().GetParentEntityId() : std::string() }
   };
}

void TransformComponent::Deserialize(const nlohmann::json& data) {
   // 不足フィールドは現在値を保つ部分更新方式にし、旧シーン形式やComponent単体保存を受け入れる。
   auto readVec3 = [](const nlohmann::json& source, const char* key, Vector3& out) {
	  if (!source.contains(key) || !source.at(key).is_array()) {
		 return;
	  }

	  const auto& array = source.at(key);
	  if (array.size() != 3) {
		 return;
	  }

	  out.x = array[0].get<float>();
	  out.y = array[1].get<float>();
	  out.z = array[2].get<float>();
	  };

   Vector3 translation = transform.translation;
   Vector3 rotationEuler = transform.rotation;
   Vector3 scale = transform.scale;
   Quaternion rotationQuaternion = transform.rotationQuaternion;
   bool hasRotationEuler = false;
   bool hasRotationQuaternion = false;
   // rotationSourceのない旧データはEulerを正本として扱う。
   Transform::RotationSource rotationSource = Transform::RotationSource::Euler;

   if (data.contains("translation")) {
	  readVec3(data, "translation", translation);
   }

   if (data.contains("rotation")) {
	  readVec3(data, "rotation", rotationEuler);
	  hasRotationEuler = true;
   }

   if (data.contains("scale")) {
	  readVec3(data, "scale", scale);
   }

   if (data.contains("rotationQuaternion") && data.at("rotationQuaternion").is_array()) {
	  const auto& quaternionArray = data.at("rotationQuaternion");
	  if (quaternionArray.size() == 4) {
		 rotationQuaternion.x = quaternionArray[0].get<float>();
		 rotationQuaternion.y = quaternionArray[1].get<float>();
		 rotationQuaternion.z = quaternionArray[2].get<float>();
		 rotationQuaternion.w = quaternionArray[3].get<float>();
		 hasRotationQuaternion = true;
	  }
   }

   if (data.contains("rotationSource") && data.at("rotationSource").is_string()) {
	  const auto source = data.at("rotationSource").get<std::string>();
	  if (source == "quaternion") {
		 rotationSource = Transform::RotationSource::Quaternion;
	  }
   }

   transform.translation = translation;
   transform.scale = scale;

   // 指定された正本だけをsetterへ渡し、setter側でEuler/Quaternionのキャッシュを同期する。
   if (rotationSource == Transform::RotationSource::Quaternion) {
	  if (hasRotationQuaternion) {
		 transform.SetRotationQuaternion(rotationQuaternion);
      } else if (hasRotationEuler) {
         // 移行途中のデータでQuaternion本体が欠けていても、Eulerから姿勢を復元して
         // 以後はQuaternion正本として扱える状態へ整える。
         transform.SetRotationEuler(rotationEuler);
		 transform.rotationSource = Transform::RotationSource::Quaternion;
	  }
   } else if (hasRotationEuler) {
	  transform.SetRotationEuler(rotationEuler);
   }

   if (data.contains("useParentMatrix") && data.at("useParentMatrix").is_boolean()) {
	  useParentMatrix = data.at("useParentMatrix").get<bool>();
   }

   // parentObjectNameは安定Entity ID導入前の互換フィールド。Sceneロード後の参照解決でIDへ移行する。
   if (data.contains("parentObjectName") && data.at("parentObjectName").is_string()) {
	  parentObjectName = data.at("parentObjectName").get<std::string>();
	  useParentMatrix = !parentObjectName.empty();
   }

   // 現行IDが存在する場合は旧名前参照より優先し、Owner側の循環検査を通して設定する。
   if (data.contains("parentEntityId") && data.at("parentEntityId").is_string() && HasOwner()) {
	  GetOwner().SetParentEntityId(data.at("parentEntityId").get<std::string>());
	  useParentMatrix = !GetOwner().GetParentEntityId().empty();
   }
}

#ifdef USE_IMGUI
void TransformComponent::DrawInspector() {
   const std::string header = MakeObjectComponentHeaderLabel(GetTypeName());
   if (!ImGui::CollapsingHeader(header.c_str())) {
	  return;
   }
   Vector3& scale = transform.scale;
   ImGuiHelper::DrawVec3Control(
      ImGuiHelper::Localize({ "スケール", "Scale" }),
      scale,
      1.0f,
      ImGuiHelper::kDefaultColumnWidth,
      0.1f,
      0.1f,
      10.0f);

   Vector3 rotationEuler = transform.GetActiveEuler();
   if (ImGuiHelper::DrawEulerDegreesControl(
      ImGuiHelper::Localize({ "回転 (deg)", "Rotation (deg)" }),
      rotationEuler,
      0.0f,
      ImGuiHelper::kDefaultColumnWidth,
      0.1f)) {
	  // TransformはEuler/Quaternionのどちらを描画に使うかを保持しているため、setter経由で両方を同期する。
	  transform.SetRotationQuaternion(rotationEuler.ToQuaternion().Normalize());
   }

   Vector3& position = transform.translation;
   ImGuiHelper::DrawVec3Control(
      ImGuiHelper::Localize({ "位置", "Position" }),
      position,
      0.0f,
      ImGuiHelper::kDefaultColumnWidth,
      0.1f);

   // Transform InspectorとGizmoの座標系/スナップ設定を同じ欄へまとめる。
   if (auto* currentScene = BaseScene::GetCurrentScene()) {
      if (auto* editorContext = currentScene->GetEditorSceneContext()) {
         editorContext->DrawGizmoInspectorControls();
      }
   }

   ImGui::Spacing();
   ImGui::Separator();
   ImGui::Spacing();

   std::string parentEntityId = HasOwner() ? GetOwner().GetParentEntityId() : std::string();
   bool parentEnabled = !parentEntityId.empty();
   if (ImGuiHelper::DrawCheckbox(ImGuiHelper::Localize({ "親を使用", "Use Parent" }), parentEnabled)) {
	  useParentMatrix = parentEnabled;
      if (!useParentMatrix) {
         // 親を無効化した時点で新旧両方の参照を消し、後のScene解決で再接続されないようにする。
         if (HasOwner()) {
			GetOwner().SetParentEntityId({});
		 }
		 parentObjectName.clear();
	  }
   }

   if (ImGuiHelper::DrawInputString(
      ImGuiHelper::Localize({ "親Entity ID", "Parent Entity ID" }),
      parentEntityId) && HasOwner()) {
	  if (GetOwner().SetParentEntityId(parentEntityId)) {
		 useParentMatrix = !parentEntityId.empty();
		 parentObjectName.clear();
	  }
   }
}
#endif

}
