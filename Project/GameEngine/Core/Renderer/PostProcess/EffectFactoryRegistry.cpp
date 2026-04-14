#include "EffectFactoryRegistry.h"
#include "PostProcess.h"

namespace GameEngine {

void EffectFactoryRegistry::RegisterFactory(const std::string& className, Factory factory) {
   factories_[className] = std::move(factory);
}

std::unique_ptr<PostProcess> EffectFactoryRegistry::Create(const std::string& className) const {
   auto it = factories_.find(className);
   if (it == factories_.end()) {
      return nullptr;
   }

   return it->second();
}

}
