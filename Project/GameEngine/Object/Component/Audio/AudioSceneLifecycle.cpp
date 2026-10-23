#include "GameEngine/pch.h"
#include "GameEngine/Object/Component/Audio/AudioSceneLifecycle.h"
#include "GameEngine/Object/Component/Audio/AudioSourceComponent.h"
#include "GameEngine/Object/Component/Audio/BgmComponent.h"
#include "GameEngine/Object/Object.h"
#include <algorithm>
#include "GameEngine/Scene/SceneWorld.h"

namespace GameEngine {
namespace {
std::vector<Object*> SortedObjects(SceneWorld& world) {
   auto objects = world.GetObjectStore().GetObjects();
   std::sort(objects.begin(), objects.end(),
      [](const Object* a, const Object* b) { return a->GetEntityId() < b->GetEntityId(); });
   return objects;
}
}

bool HasSceneBgmRequest(SceneWorld& world) {
   for (Object* object : SortedObjects(world)) {
      if (auto* bgm = object->GetComponent<BgmComponent>(); bgm && bgm->HasStartupTrack()) {
         return true;
      }
   }

   return false;
}

void BeginSceneAudio(SceneWorld& world) {
   auto objects = SortedObjects(world);
   BgmComponent* selectedBgm = nullptr;

   for (Object* object : objects) {
      if (auto* source = object->GetComponent<AudioSourceComponent>()) {
         source->BeginRuntime();
      }

      if (auto* bgm = object->GetComponent<BgmComponent>(); bgm && bgm->HasStartupTrack()) {
         if (!selectedBgm) {
            selectedBgm = bgm;
         } else {
            Logger::Warning("Multiple active BGM components; lowest entity ID wins");
         }
      }
   }

   if (selectedBgm) {
      selectedBgm->BeginRuntime();
   }
}
}
