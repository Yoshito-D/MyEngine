#pragma once
#include <memory>
#include <string>
#include "BaseScene.h"

namespace GameEngine {
class BaseScene;
class ISceneFactory;

/// @brief シーンマネージャークラス
class SceneManager {
public:
   /// @brief コンストラクタ
   /// @param factory シーンファクトリー
   /// @param context エンジンコンテキスト
   SceneManager(ISceneFactory* factory)
	  : factory_(factory) {}

   ~SceneManager() = default;

   /// @brief シーンの切り替え
   /// @param name 切り替えるシーンの名前
   void ChangeScene(const std::string& name);

   /// @brief シーンの切り替え
   /// @param newScene 新しいシーンのポインタ
   void ChangeScene(std::unique_ptr<BaseScene> newScene);

   /// @brief 現在のシーンの更新
   void Update();

   /// @brief 現在のシーンを描画
   void Draw();

   /// @brief 終了処理
   void Finalize();

   /// @brief シーンを切り替えるかどうかをチェック
   void CheckSceneChange();

private:
   std::unique_ptr<BaseScene> currentScene_;
   ISceneFactory* factory_ = nullptr;
   std::string currentSceneName_ = "";
};
}