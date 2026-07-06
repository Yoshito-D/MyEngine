#include "pch.h"
#include "TransformComponent.h"
#include "ComponentRegistry.h"
#include "Object.h"

namespace {
const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
   GameEngine::TransformComponent::kTypeName,
   [](GameEngine::Object& o) -> GameEngine::IObjectComponent* { return o.AddComponent<GameEngine::TransformComponent>(); },
   GameEngine::TransformComponent::kDisplayName
);
}

#ifdef USE_IMGUI
#include "Utility/ImGuiHelper.h"
#include <imgui.h>
#endif

namespace GameEngine {

const char* TransformComponent::GetTypeName() const {
   return "TransformComponent";
}

TransformationMatrix* TransformComponent::EnsureTransformationMatrix() {
   if (!transformationMatrix_) {
	  transformationMatrix_ = std::make_unique<TransformationMatrix>();
	  transformationMatrix_->Create();
   }

   return transformationMatrix_.get();
}

void TransformComponent::SetWorldMatrixOverride(const Matrix4x4& worldMatrix) {
   worldMatrixOverride_ = worldMatrix;
   hasWorldMatrixOverride_ = true;
}

void TransformComponent::ClearWorldMatrixOverride() {
   worldMatrixOverride_ = MakeIdentity4x4();
   hasWorldMatrixOverride_ = false;
}

nlohmann::json TransformComponent::Serialize() const {
   const Vector3 activeEuler = transform.GetActiveEuler();
   const Quaternion activeQuaternion = transform.GetActiveQuaternion();

   return nlohmann::json{
	  { "translation", { transform.translation.x, transform.translation.y, transform.translation.z } },
	  { "rotation", { activeEuler.x, activeEuler.y, activeEuler.z } },
	  { "rotationQuaternion", { activeQuaternion.x, activeQuaternion.y, activeQuaternion.z, activeQuaternion.w } },
	  { "rotationSource", transform.IsUsingQuaternion() ? "quaternion" : "euler" },
	  { "scale", { transform.scale.x, transform.scale.y, transform.scale.z } },
	  { "useParentMatrix", useParentMatrix },
	  { "parentObjectName", parentObjectName }
   };
}

void TransformComponent::Deserialize(const nlohmann::json& data) {
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

   if (rotationSource == Transform::RotationSource::Quaternion) {
	  if (hasRotationQuaternion) {
		 transform.SetRotationQuaternion(rotationQuaternion);
	  } else if (hasRotationEuler) {
		 transform.SetRotationEuler(rotationEuler);
		 transform.rotationSource = Transform::RotationSource::Quaternion;
	  }
   } else if (hasRotationEuler) {
	  transform.SetRotationEuler(rotationEuler);
   }

   if (data.contains("useParentMatrix") && data.at("useParentMatrix").is_boolean()) {
	  useParentMatrix = data.at("useParentMatrix").get<bool>();
   }

   if (data.contains("parentObjectName") && data.at("parentObjectName").is_string()) {
	  parentObjectName = data.at("parentObjectName").get<std::string>();
	  useParentMatrix = !parentObjectName.empty();
   }
}

#ifdef USE_IMGUI
void TransformComponent::DrawInspector() {
   const std::string header = MakeObjectComponentHeaderLabel(GetTypeName());
   if (!ImGui::CollapsingHeader(header.c_str())) {
	  return;
   }
   Vector3& scale = transform.scale;
   ImGuiHelper::DrawVec3Control(ImGuiHelper::Localize({ "スケール", "Scale" }), scale, 1.0f, 120.0f, 0.1f, 0.1f, 10.0f);

   Vector3 rotationEuler = transform.GetActiveEuler();
   if (ImGuiHelper::DrawEulerDegreesControl(ImGuiHelper::Localize({ "回転 (deg)", "Rotation (deg)" }), rotationEuler, 0.0f, 120.0f, 0.1f)) {
	  // TransformはEuler/Quaternionのどちらを描画に使うかを保持しているため、setter経由で両方を同期する。
	  transform.SetRotationQuaternion(rotationEuler.ToQuaternion().Normalize());
   }

   Vector3& position = transform.translation;
   ImGuiHelper::DrawVec3Control(ImGuiHelper::Localize({ "位置", "Position" }), position, 0.0f, 120.0f, 0.1f);

   ImGui::Spacing();
   ImGui::Separator();
   ImGui::Spacing();

   bool parentEnabled = useParentMatrix;
   if (ImGuiHelper::DrawCheckbox(ImGuiHelper::Localize({ "親を使用", "Use Parent" }), parentEnabled, 120.0f)) {
	  useParentMatrix = parentEnabled;
	  if (!useParentMatrix) {
		 parentObjectName.clear();
	  }
   }

   if (ImGuiHelper::DrawInputString(ImGuiHelper::Localize({ "親", "Parent" }), parentObjectName, 256, 120.0f)) {
	  useParentMatrix = !parentObjectName.empty();
   }
}
#endif

}
