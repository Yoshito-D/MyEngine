#pragma once
#include "Scene/ISceneFactory.h"
#include "SceneCatalog.h"
#include <memory>

class MySceneFactory : public GameEngine::ISceneFactory {
public:
	/// @brief シーンカタログを指定してファクトリを作成する
	/// @param sceneCatalog 読み取りと再読み込みに使用するシーンカタログ
	explicit MySceneFactory(SceneCatalog& sceneCatalog) : sceneCatalog_(sceneCatalog) {}

	/// @brief 登録名に対応する汎用JSONシーンを生成する
	/// @param name シーンカタログ上の名前
	/// @return 生成したシーン。未登録の場合はnullptr
	std::unique_ptr<GameEngine::BaseScene> CreateScene(const std::string& name) override;

private:
	SceneCatalog& sceneCatalog_;
};
