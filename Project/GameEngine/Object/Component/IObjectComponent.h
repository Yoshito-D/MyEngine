#pragma once

#include <nlohmann/json.hpp>

namespace GameEngine {
class Object;

class IObjectComponent {
public:
   virtual ~IObjectComponent() = default;

   virtual const char* GetTypeName() const {
      return "IObjectComponent";
   }

   bool IsEnabled() const {
      return isEnabled_;
   }

   void SetEnabled(bool enabled) {
      if (isEnabled_ == enabled) {
         return;
      }

      isEnabled_ = enabled;
      if (isEnabled_) {
         OnEnable();
      } else {
         OnDisable();
      }
   }

   virtual void Update(Object& owner, float deltaTime) {
      (void)owner;
      (void)deltaTime;
   }

   virtual nlohmann::json Serialize() const {
      return nlohmann::json::object();
   }

   virtual void Deserialize(const nlohmann::json& data) {
      (void)data;
   }

   virtual void OnEnable() {}
   virtual void OnDisable() {}

#ifdef USE_IMGUI
   virtual void DrawInspector(Object& owner) {
      (void)owner;
   }
#endif

private:
   bool isEnabled_ = true;
};
}
