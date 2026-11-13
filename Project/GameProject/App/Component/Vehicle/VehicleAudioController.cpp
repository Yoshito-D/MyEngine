#include "GameProject/App/Component/Vehicle/VehicleAudioController.h"
#include "GameProject/App/Component/Vehicle/VehicleController.h"
#include "GameEngine/Object/Component/Audio/AudioSourceComponent.h"
#include "GameEngine/Object/Object.h"

namespace App {

void VehicleAudioController::ResetEvents() {
   const auto* events = HasOwner() ? GetOwner().GetComponent<VehicleEvents>() : nullptr;
   eventCursor_.Reset(events ? events->GetSnapshot() : VehicleEventSnapshot{});
}

void VehicleAudioController::OnSceneLoaded(GameEngine::SceneWorld&) {
   ResetEvents();
}

void VehicleAudioController::OnEnable() {
   ResetEvents();
}

void VehicleAudioController::OnDisable() {
   if (!HasOwner()) return;

   if (auto* source = GetOwner().GetComponent<GameEngine::AudioSourceComponent>()) {
      source->StopAll();
   }
}

void VehicleAudioController::Update(float) {
   if (!HasOwner()) return;

   auto* source = GetOwner().GetComponent<GameEngine::AudioSourceComponent>();
   const auto* events = GetOwner().GetComponent<VehicleEvents>();

   if (!source || !events) return;

   const auto& snapshot = events->GetSnapshot();
   const auto changes = eventCursor_.Read(snapshot);
   const auto* vehicle = GetOwner().GetComponent<VehicleController>();
   const bool playDrift = snapshot.isDrifting && snapshot.isGrounded && (!vehicle || vehicle->IsEnabled());

   // Playは再生中のスロットを再起動しないため、ドリフトの継続中も音が重ならない。
   // ゴール後など操作停止中に最後のドリフト状態が残っていても、ループを続けない。
   if (playDrift) {
      source->Play("drift");
   } else {
      source->Stop("drift");
   }

   // チャージ完了音はドリフト音と重ね、発動・離陸・操作停止によって条件を失った時点で止める。
   if (playDrift && snapshot.miniTurboReady) {
      source->Play("miniTurbo");
   } else {
      source->Stop("miniTurbo");
   }

   if (changes.miniTurbo) {
      source->Restart("acceleration");
   }

   if (changes.tookOff) {
      source->Restart("landing");
   }

   // 失敗時は通常の着地音と重ねず、同じ着地通知の確定済み品質から選ぶ。
   if (changes.landed) {
      const bool failed = snapshot.landingResult == LandingResult::Failure;
      source->Stop(failed ? "landing" : "miss");
      source->Restart(failed ? "miss" : "landing");
   }
}

} // namespace App
