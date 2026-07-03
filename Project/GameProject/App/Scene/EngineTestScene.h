#pragma once
#include "BaseScene.h"
#include "Model.h"
#include "Effect/ParticleSystem.h"
#include <memory>

/// @brief エンジン機能テスト用シーン（アニメーション・パーティクル等）
class EngineTestScene : public GameEngine::BaseScene {
protected:
   void OnInitialize() override;
   void OnEditorUpdate() override;

private:
   std::unique_ptr<GameEngine::Model> animCube_ = nullptr;
   std::unique_ptr<GameEngine::ParticleSystem> particleSystem_ = nullptr;
};
