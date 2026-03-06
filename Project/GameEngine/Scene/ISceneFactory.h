#pragma once
#include "BaseScene.h"
#include <memory>
#include <string>
#include "EngineContext.h"

namespace GameEngine {
/// @brief シーンファクトリインターフェース
class ISceneFactory {
public:

   /// @brief シーンを生成する
   /// @param name シーンの名前
   virtual std::unique_ptr<BaseScene> CreateScene(const std::string& name) = 0;

   /// @brief シーンファクトリのデストラクタ
   virtual ~ISceneFactory() = default;
};
}