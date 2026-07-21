#include "MySceneFactory.h"
#include "Scene/DataDrivenScene.h"

using namespace GameEngine;
std::unique_ptr<BaseScene> MySceneFactory::CreateScene(const std::string& name) {
	const auto scenePath = sceneCatalog_.Resolve(name);
	if (scenePath.empty()) {
		return nullptr;
	}
	return std::make_unique<DataDrivenScene>(scenePath);
}
