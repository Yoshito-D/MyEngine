#include "CharacterController.h"
#include "Object/Object.h"
#include "Framework/EngineContext.h"
#include <cmath>
#include <algorithm>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

using namespace GameEngine;

namespace App {

void CharacterController::CacheComponents() {
   // 同オーナー上の依存コンポーネントを取得
   basis_  = GetOwner().GetComponent<ScreenSpaceBasis>();
   walker_ = GetOwner().GetComponent<CharacterWalker>();
   jump_   = GetOwner().GetComponent<CharacterJump>();

   // 基底コンポーネントへカメラ参照を伝播
   if (basis_) {
      basis_->SetCamera(camera_);
      basis_->SetOrbitalBody(orbitalBody_);
      basis_->SetGravityFollowCamera(gravityFollowCamera_);
      basis_->SetPlanetLeashCamera(planetLeashCamera_);
   }
}

void CharacterController::Update(float deltaTime) {
   // オーナーが無ければ入力処理を行わない
   if (!HasOwner()) { return; }

   // 依存コンポーネントが未キャッシュなら取得
   if (!basis_ || !walker_ || !jump_) {
      CacheComponents();
   }

   // 現在の重力Upを取得（無ければワールドUp）
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   Vector3 gravityUp = { 0.0f, 1.0f, 0.0f };
   if (gravityBody) { gravityUp = gravityBody->GetCurrentUpVector(); }

   // カメラ操作入力を GravityFollowCamera へ反映
   if (gravityFollowCamera_) {
      Vector2 camInput = { 0.0f, 0.0f };

      // 右スティック入力
      Vector2 rStick = EngineContext::GetRightStick(0);
      if (std::abs(rStick.x) > inputDeadZone || std::abs(rStick.y) > inputDeadZone) {
         camInput = rStick;
      }

      // D-pad入力
      if (EngineContext::IsKeyPressed(KeyCode::Right)) { camInput.x += 1.0f; }
      if (EngineContext::IsKeyPressed(KeyCode::Left))  { camInput.x -= 1.0f; }
      if (EngineContext::IsKeyPressed(KeyCode::Up))    { camInput.y -= 1.0f; }
      if (EngineContext::IsKeyPressed(KeyCode::Down))  { camInput.y += 1.0f; }

      // 入力があれば回転デルタへ変換して適用
      if (camInput.x != 0.0f || camInput.y != 0.0f) {
         constexpr float kCamRotateScale = 150.0f;
         Vector2 delta = { camInput.x * kCamRotateScale * deltaTime,
                           camInput.y * kCamRotateScale * deltaTime };
         gravityFollowCamera_->ProcessInput(delta, 0, true);
      }
   }

   // ジャンプ入力を処理
   if (jump_ && CollectJumpInput()) {
      jump_->Jump(gravityUp);
   }

   // 移動入力を処理
   if (walker_) {
      bool isGrounded = !(jump_ && jump_->IsJumping());
      Vector2 input = CollectMoveInput();
      float inputLength = std::sqrt(input.x * input.x + input.y * input.y);

      // デッドゾーン以上なら正規化して移動適用
      if (inputLength >= inputDeadZone) {
         float normalizedLength = std::min(inputLength, 1.0f);
         Vector2 normalizedInput = {
            (input.x / inputLength) * normalizedLength,
            (input.y / inputLength) * normalizedLength
         };
         walker_->ApplyMovement(normalizedInput, gravityUp, deltaTime, isGrounded);
      } else {
         walker_->ApplyMovement({ 0.0f, 0.0f }, gravityUp, deltaTime, isGrounded);
      }
   }
}

Vector2 CharacterController::CollectMoveInput() const {
   // キーボード入力を合成
   Vector2 input = { 0.0f, 0.0f };
   if (EngineContext::IsKeyPressed(KeyCode::W)) { input.y += 1.0f; }
   if (EngineContext::IsKeyPressed(KeyCode::S)) { input.y -= 1.0f; }
   if (EngineContext::IsKeyPressed(KeyCode::A)) { input.x -= 1.0f; }
   if (EngineContext::IsKeyPressed(KeyCode::D)) { input.x += 1.0f; }

   // 左スティックが有効なら優先採用
   Vector2 stick = EngineContext::GetLeftStick(0);
   if (std::abs(stick.x) > inputDeadZone || std::abs(stick.y) > inputDeadZone) {
      input = stick;
   }
   return input;
}

bool CharacterController::CollectJumpInput() const {
   // スペース押下トリガーをジャンプ入力とする
   return EngineContext::IsKeyTriggered(KeyCode::Space);
}

#ifdef USE_IMGUI
void CharacterController::DrawInspector() {
   if (!ImGui::CollapsingHeader("CharacterController")) {
      return;
   }
   ImGui::Separator();
   ImGui::DragFloat("Input DeadZone", &inputDeadZone, 0.01f, 0.0f, 1.0f);
}
#endif

nlohmann::json CharacterController::Serialize() const {
   nlohmann::json json;
   json["inputDeadZone"] = inputDeadZone;
   return json;
}

void CharacterController::Deserialize(const nlohmann::json& data) {
   if (data.contains("inputDeadZone")) { inputDeadZone = data["inputDeadZone"]; }
}

} // namespace App
