#include "pch.h"
#include "AudioSceneLifecycle.h"
#include "AudioSourceComponent.h"
#include "BgmComponent.h"
#include "Object.h"
#include <algorithm>

namespace GameEngine {
namespace {
std::vector<Object*> SortedObjects() {
   auto objects = Object::GetRegisteredObjects();
   std::sort(objects.begin(), objects.end(), [](const Object* a, const Object* b) {
      return a->GetEntityId() < b->GetEntityId();
   });
   return objects;
}
}
bool HasSceneBgmRequest() {
   for (Object* object : SortedObjects()) {
      if (auto* bgm = object->GetComponent<BgmComponent>(); bgm && bgm->HasStartupTrack()) return true;
   }
   return false;
}
void BeginSceneAudio() {
   auto objects = SortedObjects();
   BgmComponent* selectedBgm = nullptr;
   for (Object* object : objects) {
      if (auto* source = object->GetComponent<AudioSourceComponent>()) source->BeginRuntime();
      if (auto* bgm = object->GetComponent<BgmComponent>(); bgm && bgm->HasStartupTrack()) {
         if (!selectedBgm) selectedBgm = bgm;
         else Logger::Warning("Multiple active BGM components; lowest entity ID wins");
      }
   }
   if (selectedBgm) selectedBgm->BeginRuntime();
}
}
