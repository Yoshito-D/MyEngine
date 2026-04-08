#include "pch.h"
#include "TransformComponent.h"

namespace GameEngine {

const char* TransformComponent::GetTypeName() const {
   return "TransformComponent";
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

}
