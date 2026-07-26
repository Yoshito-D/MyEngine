#include "MySceneFactory.h"
#include "Scene/DataDrivenScene.h"

using namespace GameEngine;
std::unique_ptr<BaseScene> MySceneFactory::CreateScene(const std::string& name) {
	// カタログを唯一の名前解決元にすることで、シーン名とファイル配置の重複管理を避ける。
	auto scenePath = sceneCatalog_.Resolve(name);
	if (scenePath.empty()) {
		// エディタで追加したシーンへ再起動せず遷移できるよう、未登録名だけ再読込する。
		if (!sceneCatalog_.Load()) {
			return nullptr;
		}
		scenePath = sceneCatalog_.Resolve(name);
	}
	if (scenePath.empty()) {
		return nullptr;
	}
	return std::make_unique<DataDrivenScene>(scenePath);
}
