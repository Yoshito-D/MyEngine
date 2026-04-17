#include "pch.h"
#include "ColliderComponent.h"
#include "ComponentRegistry.h"
#include "Object.h"

namespace {
   const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
      GameEngine::ColliderComponent::kTypeName,
      [](GameEngine::Object& o) -> GameEngine::IObjectComponent* { return o.AddComponent<GameEngine::ColliderComponent>(); }
   );
}
