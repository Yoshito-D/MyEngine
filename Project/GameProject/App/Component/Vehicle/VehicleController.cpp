#include "GameProject/App/Component/Vehicle/VehicleController.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Scene/SceneWorld.h"
#include "GameEngine/Scene/Camera/Core/VirtualCamera.h"

#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Graphics/Renderer/Pass/PlayerShadowPass.h"

#include "GameEngine/Object/Model/Model.h"
#include "GameEngine/Object/Component/Rendering/MeshComponent.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"

#include "GameProject/App/Component/Gravity/PlanetSwitcher.h"

#include <cmath>

#ifdef USE_IMGUI
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

using namespace GameEngine;

namespace App {

void VehicleController::SetGravityFollowCamera(GravityFollowCamera* camera) {
   const auto* ownerCamera = camera ? camera->GetOwnerCamera() : nullptr;
   gravityFollowCameraId_ = ownerCamera ? ownerCamera->GetId() : std::string{};
}

Vector3 VehicleController::GetLastMoveDirection() const {
   const auto* mover = HasOwner() ? GetOwner().GetComponent<VehicleMover>() : nullptr;
   return mover ? mover->GetLastMoveDirection() : Vector3{ 0.0f, 0.0f, 1.0f };
}

void VehicleController::Update(float deltaTime) {
   if (!HasOwner()) { return; }

   // Inspectorで依存Componentが削除・再追加されるため、利用する更新ごとに解決する。
   auto* input = GetOwner().GetComponent<VehicleInputComponent>();
   auto* mover = GetOwner().GetComponent<VehicleMover>();
   auto* jump = GetOwner().GetComponent<CharacterJump>();

   // 現在の重力Upを取得（無ければワールドUp）
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   Vector3 gravityUp = { 0.0f, 1.0f, 0.0f };
   if (gravityBody) { gravityUp = gravityBody->GetTargetUpVector(); }

   // 制御側は物理キーを知らず、意味付け済みの入力だけを消費する。
   // カメラやその部品をInspectorで削除しても、次の入力で旧メモリへアクセスしない。
   auto* world = SceneWorld::GetCurrent();
   auto* camera = world ? world->FindVirtualCamera(gravityFollowCameraId_) : nullptr;
   auto* gravityFollowCamera = camera ? camera->GetComponent<GravityFollowCamera>() : nullptr;
   if (gravityFollowCamera && input) {
      const Vector2 camInput = input->GetCameraLookInput();
      if (camInput.x != 0.0f || camInput.y != 0.0f) {
         constexpr float kCamRotateScale = 150.0f;
         Vector2 delta = { camInput.x * kCamRotateScale * deltaTime,
                           camInput.y * kCamRotateScale * deltaTime };
         gravityFollowCamera->ProcessInput(delta, 0, true);
      }
   }

   // 接地判定（ジャンプ中でなければ接地扱い）
   bool isGrounded = !(jump && jump->IsJumping());

   // ジャンプ入力
   if (jump && input && input->IsJumpTriggered()) {
	  jump->Jump(gravityUp);
   }

   // 移動・姿勢入力を VehicleMover へ委譲
   if (mover) {
	  const float steerInput = input ? input->GetSteerInput() : 0.0f;
	  const float rollInput = input ? input->GetRollInput() : 0.0f;
	  const float pitchInput = input ? input->GetPitchInput() : 0.0f;
	  const bool driftInput = input && input->IsDriftHeld();
	  mover->ApplyMovement(steerInput, rollInput, pitchInput, driftInput, isGrounded, gravityUp, deltaTime);
   }
}

bool VehicleController::TryBuildPlayerShadowFrameData(
   GameEngine::Camera* camera,
   GameEngine::PlayerShadowFrameData& outFrameData) {
   outFrameData = {};

   // レース中の無効化は操作の停止であり、影の描画対象からは外さない。
   if (!HasOwner() || !camera) {
      return false;
   }

   auto* player =
      dynamic_cast<GameEngine::Model*>(&GetOwner());

   auto* switcher =
      GetOwner().GetComponent<PlanetSwitcher>();

   if (!player || !switcher || !switcher->IsEnabled()) {
      return false;
   }

   GameEngine::Model* receiver = nullptr;
   GameEngine::Vector3 center{};
   float radius = 0.0f;

   if (!switcher->TryGetLandingPlanet(
      receiver, center, radius) ||
      receiver == player) {
      return false;
   }

   // プレイヤーの位置は、親の変換を含むワールド位置を使用する。
   const auto world = player->GetWorldMatrix();

   const GameEngine::Vector3 position{
       world.m[3][0],
       world.m[3][1],
       world.m[3][2]
   };

   if (!std::isfinite(position.x) ||
      !std::isfinite(position.y) ||
      !std::isfinite(position.z)) {
      return false;
   }

   outFrameData.player = player;
   outFrameData.receiver = receiver;
   outFrameData.camera = camera;
   outFrameData.playerPosition = position;
   outFrameData.planetCenter = center;
   outFrameData.planetRadius = radius;

   return true;
}

void VehicleController::SubmitPlayerShadow(
   GameEngine::Camera* camera) {
   GameEngine::PlayerShadowFrameData data{};

   if (!TryBuildPlayerShadowFrameData(camera, data)) {
      GameEngine::EngineContext::ClearPlayerShadowFrameData();
      return;
   }

   const auto prepareModel =
      [camera](GameEngine::Model* model) {
      auto* mesh =
         model->GetComponent<GameEngine::MeshComponent>();

      auto* transform =
         model->GetComponent<GameEngine::TransformComponent>();

      if (!mesh || !transform) {
         return false;
      }

      // プリミティブは必要に応じてメッシュを生成する。
      // ファイルモデルは読み込み済みのアセットを使用する。
      if (mesh->GetSourceType() ==
         GameEngine::MeshComponent::SourceType::Primitive) {
         if (!mesh->EnsureMesh()) {
            return false;
         }
      } else {
         if (!mesh->GetModelAsset()) {
            return false;
         }
      }

      // 通常描画されないモデルにも、
      // 現在の親行列と描画カメラを反映する。
      transform->ResolveParentRelation();

      model->UpdateMatrix(camera);

      return transform->GetTransformationMatrix() != nullptr;
      };

   if (!prepareModel(data.player) ||
      !prepareModel(data.receiver)) {
      GameEngine::EngineContext::ClearPlayerShadowFrameData();
      return;
   }

   GameEngine::EngineContext::SetPlayerShadowFrameData(data);
}

#ifdef USE_IMGUI
void VehicleController::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) { return; }
   ImGui::Separator();
   ImGui::TextUnformatted(Tr("入力は VehicleInputComponent から取得します", "Input is provided by VehicleInputComponent"));
}
#endif

nlohmann::json VehicleController::Serialize() const {
   return nlohmann::json::object();
}

void VehicleController::Deserialize(const nlohmann::json& data) {
   (void)data;
}

} // namespace App
