#include "VehicleSpeedGaugeUIComponent.h"

#include "RaceManagerComponent.h"
#include "../Gravity/GravityBody.h"
#include "../Vehicle/VehicleGroundMover.h"
#include "Framework/EngineContext.h"
#include "Object/Component/MaterialComponent.h"
#include "Object/Component/RenderComponent.h"
#include "Object/Component/UI/UITextComponent.h"
#include "Object/Object.h"
#include "Object/Sprite/Sprite.h"
#include "Scene/SceneWorld.h"
#include "Core/Graphics/Texture.h"
#include <algorithm>
#include <cmath>
#include <format>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#include "Utility/ImGuiHelper.h"
#endif

namespace App {

void VehicleSpeedGaugeUIComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   raceManager_ = nullptr;
   gravityBody_ = nullptr;
   groundMover_ = nullptr;
   gaugeSprite_ = HasOwner() ? dynamic_cast<GameEngine::Sprite*>(&GetOwner()) : nullptr;
   gaugeRender_ = gaugeSprite_ ? gaugeSprite_->GetComponent<GameEngine::RenderComponent>() : nullptr;
   frameObject_ = sceneWorld.FindObjectById(frameObjectId_);
   speedText_ = nullptr;

   if (auto* managerObject = sceneWorld.FindObjectById(raceManagerId_)) {
      raceManager_ = managerObject->GetComponent<RaceManagerComponent>();
   }
   if (auto* playerObject = sceneWorld.FindObjectById(playerObjectId_)) {
      gravityBody_ = playerObject->GetComponent<GravityBody>();
      groundMover_ = playerObject->GetComponent<VehicleGroundMover>();
   }
   if (auto* textObject = sceneWorld.FindObjectById(speedTextObjectId_)) {
      speedText_ = textObject->GetComponent<GameEngine::UITextComponent>();
   }

   if (gaugeSprite_) {
      gaugeSprite_->SetAnchorPoint({ 0.0f, 0.0f });
      // 右端を原点として画像を左右反転し、幅の増加を左方向へ伸ばす。
      gaugeSprite_->SetFlipX(true);
      gaugeSprite_->SetSize({ gaugeWidth_, gaugeHeight_ });

      if (const auto* material = gaugeSprite_->GetComponent<GameEngine::MaterialComponent>()) {
         if (const auto* texture = GameEngine::EngineContext::GetTexture(material->GetTextureName())) {
            textureWidth_ = static_cast<float>(texture->GetWidth());
            textureHeight_ = static_cast<float>(texture->GetHeight());
         }
      }
      gaugeSprite_->SetTextureUV({ 0.0f, 0.0f }, { textureWidth_, textureHeight_ });
   }

   displayedRatio_ = 0.0f;
   SetHudVisible(false);
}

void VehicleSpeedGaugeUIComponent::Update(float deltaTime) {
   const bool isFinished = raceManager_ &&
      raceManager_->GetState() == RaceManagerComponent::State::Finished;
   const bool canDisplay = gaugeSprite_ && (gravityBody_ || groundMover_) && !isFinished;
   if (!canDisplay) {
      SetHudVisible(false);
      return;
   }

   // 空中の垂直速度も含む実移動速度を優先し、地上移動だけの値に固定されないようにする。
   const float speed = gravityBody_
      ? std::max(gravityBody_->GetVelocity().Length(), 0.0f)
      : std::max(groundMover_->GetCurrentSpeed(), 0.0f);
   const float maximumSpeed = groundMover_
      ? std::max(groundMover_->maxSpeed, 0.001f)
      : 40.0f;
   const float targetRatio = std::clamp(speed / maximumSpeed, 0.0f, 1.0f);
   const float interpolation = 1.0f - std::exp(-std::max(response_, 0.0f) * std::max(deltaTime, 0.0f));
   displayedRatio_ += (targetRatio - displayedRatio_) * interpolation;
   displayedRatio_ = std::clamp(displayedRatio_, 0.0f, 1.0f);

   // Quad幅と参照UV幅を同じ比率にし、画像を潰さず左から右へ表示する。
   const float visibleRatio = std::max(displayedRatio_, 0.001f);
   gaugeSprite_->SetSize({ gaugeWidth_ * visibleRatio, gaugeHeight_ });
   gaugeSprite_->SetTextureUV(
      { 0.0f, 0.0f },
      { textureWidth_ * visibleRatio, textureHeight_ });

   if (speedText_) {
      speedText_->SetText(std::format("{:.0f} km/h", speed * unitToKmh_));
   }
   SetHudVisible(true);
}

void VehicleSpeedGaugeUIComponent::SetHudVisible(bool visible) {
   if (gaugeRender_) {
      gaugeRender_->visible = visible && displayedRatio_ > 0.001f;
   }
   if (frameObject_) {
      if (auto* render = frameObject_->GetComponent<GameEngine::RenderComponent>()) {
         render->visible = visible;
      }
   }
   if (speedText_ && !visible) {
      speedText_->SetText("");
   }
}

nlohmann::json VehicleSpeedGaugeUIComponent::Serialize() const {
   return nlohmann::json{
      { "raceManagerId", raceManagerId_ },
      { "playerObjectId", playerObjectId_ },
      { "frameObjectId", frameObjectId_ },
      { "speedTextObjectId", speedTextObjectId_ },
      { "gaugeWidth", gaugeWidth_ },
      { "gaugeHeight", gaugeHeight_ },
      { "response", response_ },
      { "unitToKmh", unitToKmh_ }
   };
}

void VehicleSpeedGaugeUIComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }
   auto readString = [&data](const char* key, std::string& value) {
      if (data.contains(key) && data.at(key).is_string()) {
         value = data.at(key).get<std::string>();
      }
   };
   auto readPositiveFloat = [&data](const char* key, float& value) {
      if (data.contains(key) && data.at(key).is_number()) {
         value = std::max(data.at(key).get<float>(), 0.001f);
      }
   };

   readString("raceManagerId", raceManagerId_);
   readString("playerObjectId", playerObjectId_);
   readString("frameObjectId", frameObjectId_);
   readString("speedTextObjectId", speedTextObjectId_);
   readPositiveFloat("gaugeWidth", gaugeWidth_);
   readPositiveFloat("gaugeHeight", gaugeHeight_);
   readPositiveFloat("response", response_);
   readPositiveFloat("unitToKmh", unitToKmh_);
}

#ifdef USE_IMGUI
void VehicleSpeedGaugeUIComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   constexpr float kColumnWidth = 150.0f;
   GameEngine::ImGuiHelper::DrawInputString("Race Manager ID", raceManagerId_, GameEngine::ImGuiHelper::kDefaultTextBufferSize, kColumnWidth);
   GameEngine::ImGuiHelper::DrawInputString("Player Object ID", playerObjectId_, GameEngine::ImGuiHelper::kDefaultTextBufferSize, kColumnWidth);
   GameEngine::ImGuiHelper::DrawInputString("Frame Object ID", frameObjectId_, GameEngine::ImGuiHelper::kDefaultTextBufferSize, kColumnWidth);
   GameEngine::ImGuiHelper::DrawInputString("Speed Text Object ID", speedTextObjectId_, GameEngine::ImGuiHelper::kDefaultTextBufferSize, kColumnWidth);
   ImGui::DragFloat("Gauge Width", &gaugeWidth_, 1.0f, 1.0f, 4096.0f);
   ImGui::DragFloat("Gauge Height", &gaugeHeight_, 1.0f, 1.0f, 4096.0f);
   ImGui::DragFloat("Response", &response_, 0.1f, 0.001f, 100.0f);
   ImGui::DragFloat("Unit to km/h", &unitToKmh_, 0.1f, 0.001f, 100.0f);
   ImGui::Text("Resolved: Body=%s Mover=%s Gauge=%s Frame=%s Text=%s",
      gravityBody_ ? "true" : "false",
      groundMover_ ? "true" : "false",
      gaugeSprite_ ? "true" : "false",
      frameObject_ ? "true" : "false",
      speedText_ ? "true" : "false");
}
#endif

} // namespace App
