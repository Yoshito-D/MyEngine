#include "GravityAttractorLink.h"
#include "GravityBody.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void GravityAttractorLink::SetAttractor(GravityAttractor* attractor) {
   if (!attractor || !attractor->HasOwner()) {
      attractorEntityId_.clear();
      attractorObjectName_.clear();
      attractorTypeName_.clear();
      return;
   }

   const auto& owner = attractor->GetOwner();
   attractorEntityId_ = owner.GetEntityId();
   attractorObjectName_ = owner.GetObjectName();
   attractorTypeName_ = attractor->GetTypeName();
}

GravityAttractor* GravityAttractorLink::ResolveAttractor() const {
   if (attractorTypeName_.empty()) {
      return nullptr;
   }

   GameEngine::Object* object = nullptr;
   if (!attractorEntityId_.empty()) {
      object = GameEngine::Object::FindByEntityId(attractorEntityId_);
   }
   if (!object && !attractorObjectName_.empty()) {
      object = GameEngine::Object::FindByObjectName(attractorObjectName_);
   }
   if (!object) {
      return nullptr;
   }

   return dynamic_cast<GravityAttractor*>(object->GetComponentByTypeName(attractorTypeName_));
}

void GravityAttractorLink::Update(float) {
   if (!HasOwner()) { return; }

   // 適用対象の GravityBody と現在位置を取得
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   auto* transform   = GetOwner().GetComponent<GameEngine::TransformComponent>();
   if (!gravityBody || !transform) { return; }

   // 接続先が削除・無効化・圏外になった場合、前フレームの加速度を残さない。
   auto* attractor = ResolveAttractor();
   if (!attractor || !attractor->ApplyTo(*gravityBody, transform->transform.translation)) {
      gravityBody->SetGravity({ 0.0f, 0.0f, 0.0f });
   }
}

#ifdef USE_IMGUI
void GravityAttractorLink::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGui::Separator();
   ImGui::Text("%s: %s", Tr("アトラクター", "Attractor"), ResolveAttractor() ? Tr("設定済み", "Set") : Tr("なし", "None"));
}
#endif

} // namespace App
