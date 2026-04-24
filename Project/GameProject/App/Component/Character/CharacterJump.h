#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"

namespace App {

/// @brief ジャンプ初速を GravityBody の速度に加算するコンポーネント
class CharacterJump final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "CharacterJump";
   const char* GetTypeName() const override { return kTypeName; }

   void Update(float deltaTime) override { (void)deltaTime; }

   void Jump(const GameEngine::Vector3& gravityUp);

   bool IsJumping()    const { return isJumping_; }
   void NotifyLanded()       { isJumping_ = false; }

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   nlohmann::json Serialize() const override;
   void Deserialize(const nlohmann::json& data) override;

public:
   float jumpStrength = 9.0f;

private:
   bool isJumping_ = false;
};

} // namespace App
