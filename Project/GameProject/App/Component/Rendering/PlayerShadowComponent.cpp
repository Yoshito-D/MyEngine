#include "GameProject/App/Component/Rendering/PlayerShadowComponent.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Graphics/Renderer/Pass/PlayerShadowPass.h"
#include "GameEngine/Object/Model/Model.h"
#include "GameEngine/Object/Component/Rendering/MeshComponent.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameProject/App/Component/Gravity/PlanetSwitcher.h"
#include <cmath>

namespace App {
bool PlayerShadowComponent::TryBuildPlayerShadowFrameData(
   GameEngine::Camera* camera, GameEngine::PlayerShadowFrameData& outFrameData) {
   outFrameData = {};

   // レース中の無効化は操作の停止であり、影の描画対象からは外さない。
   if (!HasOwner() || !camera) {
      return false;
   }

   auto* player = dynamic_cast<GameEngine::Model*>(&GetOwner());

   auto* switcher = GetOwner().GetComponent<PlanetSwitcher>();

   if (!player || !switcher || !switcher->IsEnabled()) {
      return false;
   }

   GameEngine::Model* receiver = nullptr;
   GameEngine::Vector3 center{};
   float radius = 0.0f;

   if (!switcher->TryGetLandingPlanet(receiver, center, radius) || receiver == player) {
      return false;
   }

   // プレイヤーの位置は、親の変換を含むワールド位置を使用する。
   const auto world = player->GetWorldMatrix();

   const GameEngine::Vector3 position{world.m[3][0], world.m[3][1], world.m[3][2]};

   if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
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

void PlayerShadowComponent::SubmitPlayerShadow(GameEngine::Camera* camera) {
   GameEngine::PlayerShadowFrameData data{};

   if (!TryBuildPlayerShadowFrameData(camera, data)) {
      GameEngine::EngineContext::ClearPlayerShadowFrameData();
      return;
   }

   const auto prepareModel = [camera](GameEngine::Model* model) {
      auto* mesh = model->GetComponent<GameEngine::MeshComponent>();

      auto* transform = model->GetComponent<GameEngine::TransformComponent>();

      if (!mesh || !transform) {
         return false;
      }

      // プリミティブは必要に応じてメッシュを生成する。
      // ファイルモデルは読み込み済みのアセットを使用する。
      if (mesh->GetSourceType() == GameEngine::MeshComponent::SourceType::Primitive) {
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

      model->UpdateMatrix(camera);

      return transform->GetTransformationMatrix() != nullptr;
   };

   if (!prepareModel(data.player) || !prepareModel(data.receiver)) {
      GameEngine::EngineContext::ClearPlayerShadowFrameData();
      return;
   }

   GameEngine::EngineContext::SetPlayerShadowFrameData(data);
}
} // namespace App
