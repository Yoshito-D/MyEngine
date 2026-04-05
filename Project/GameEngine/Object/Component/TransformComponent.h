#pragma once
#include "IObjectComponent.h"
#include "Utility/VectorMath.h"
#include <string>

namespace GameEngine {
class TransformComponent final : public IObjectComponent {
public:
   const char* GetTypeName() const override {
      return "TransformComponent";
   }

   nlohmann::json Serialize() const override {
      return nlohmann::json{
         { "translation", { transform.translation.x, transform.translation.y, transform.translation.z } },
         { "rotation", { transform.rotation.x, transform.rotation.y, transform.rotation.z } },
         { "scale", { transform.scale.x, transform.scale.y, transform.scale.z } },
         { "useParentMatrix", useParentMatrix },
         { "parentObjectName", parentObjectName }
      };
   }

   void Deserialize(const nlohmann::json& data) override {
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

      readVec3(data, "translation", transform.translation);
      readVec3(data, "rotation", transform.rotation);
      readVec3(data, "scale", transform.scale);

      if (data.contains("useParentMatrix") && data.at("useParentMatrix").is_boolean()) {
         useParentMatrix = data.at("useParentMatrix").get<bool>();
      }

      if (data.contains("parentObjectName") && data.at("parentObjectName").is_string()) {
         parentObjectName = data.at("parentObjectName").get<std::string>();
         useParentMatrix = !parentObjectName.empty();
      }
   }

   Transform transform = {};
   Matrix4x4 parentMatrix = MakeIdentity4x4();
   bool useParentMatrix = false;
   std::string parentObjectName;
};
}
