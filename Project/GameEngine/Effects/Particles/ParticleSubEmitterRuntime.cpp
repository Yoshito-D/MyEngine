#include "GameEngine/pch.h"
#include "GameEngine/Effects/Particles/ParticleSubEmitterRuntime.h"
#include "GameEngine/Effects/Particles/ParticleSystem.h"
#include <algorithm>

namespace GameEngine {

namespace {
// 子の死亡イベントが相互参照してもワールド内の生成が無制限に増えないようにする。
constexpr size_t kMaxRuntimeSubEmitters = 256;
}

ParticleSubEmitterRuntime::ParticleSubEmitterRuntime(SceneWorld& world) : world_(world) {
}

ParticleSubEmitterRuntime::~ParticleSubEmitterRuntime() = default;

void ParticleSubEmitterRuntime::Queue(const std::string& effectPath, const Vector3& position) {
   if (effectPath.empty()) return;

   pendingEvents_.push_back({effectPath, position});
}

void ParticleSubEmitterRuntime::Retire(std::unique_ptr<ParticleSystem> child) {
   // レジストリは非所有なので直ちに解除し、GPU資源だけをフレーム境界まで保持する。
   ParticleSystem::UnregisterParticleSystem(child.get());
   retiredChildren_.push_back(std::move(child));
}

void ParticleSubEmitterRuntime::ProcessPending() {
   for (auto& child : children_) {
      if (child && !child->IsPlaying() && child->GetActiveParticleCount() == 0) {
         Retire(std::move(child));
      }
   }

   std::erase(children_, nullptr);

   // 今回分を引き取り、生成中に新しい要求が生じても同じ走査へ再入しない。
   auto pending = std::move(pendingEvents_);
   pendingEvents_.clear();

   for (const PendingEvent& event : pending) {
      if (children_.size() >= kMaxRuntimeSubEmitters) {
         break;
      }

      auto child = std::make_unique<ParticleSystem>();
      child->SetWorld(&world_);
      child->SetEditorInspectable(false);

      if (!child->LoadFromJson(event.effectPath)) {
         continue;
      }

      auto shape = child->DescribeShapeModule();
      Transform transform = shape.GetTransform();
      transform.translation += event.position;
      child->ApplyEmitterPose(transform);
      child->Create();

      if (!child->IsPlaying()) {
         child->Play();
      }

      children_.push_back(std::move(child));
   }
}

void ParticleSubEmitterRuntime::Clear() {
   pendingEvents_.clear();

   for (auto& child : children_) {
      Retire(std::move(child));
   }

   children_.clear();
}

void ParticleSubEmitterRuntime::FinishFrame() {
   retiredChildren_.clear();
}

} // namespace GameEngine
