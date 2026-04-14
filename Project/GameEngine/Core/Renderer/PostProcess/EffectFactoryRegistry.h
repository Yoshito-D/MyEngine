#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace GameEngine {

class PostProcess;

class EffectFactoryRegistry {
public:
   using Factory = std::function<std::unique_ptr<PostProcess>()>;

   void RegisterFactory(const std::string& className, Factory factory);
   std::unique_ptr<PostProcess> Create(const std::string& className) const;

private:
   std::unordered_map<std::string, Factory> factories_;
};

}
