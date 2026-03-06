#include "GameSceneRenderer.h"
#include "Framework/EngineContext.h"
#include "Object/Model/Model.h"
#include "Object/Sprite/Sprite.h"
#include "Core/Graphics/Texture.h"
#include "Core/Graphics/PipelineState.h"
#include "Effect/ParticleSystem.h"
#include "LevelEditor.h"
#include "../GameObject/Player/Player.h"
#include "../GameObject/Planet/Planet.h"
#include "../GameObject/Star/Star.h"
#include "../GameObject/Rabbit/Rabbit.h"
#include "../GameObject/Box/Box.h"

using namespace GameEngine;

void GameSceneRenderer::DrawUI(
   Sprite* moveUISprite,
   Sprite* jumpUISprite,
   Sprite* cameraUISprite,
   Sprite* spinUISprite,
   bool showUI
) {
   // 演出中はUIを表示しない
   if (!showUI) {
	  return;
   }

   auto moveTex = EngineContext::GetTexture("moveUI");
   auto jumpTex = EngineContext::GetTexture("jumpUI");
   auto cameraTex = EngineContext::GetTexture("cameraUI");
   auto spinTex = EngineContext::GetTexture("spinUI");

   // カメラ設定
   EngineContext::SetActiveCamera(0);

   // UIの描画
   if (moveUISprite) {
	  EngineContext::DrawUI(moveUISprite, moveTex, Sprite::AnchorPoint::BottomLeft, BlendMode::kBlendModeNormal);
   }

   if (jumpUISprite) {
	  EngineContext::DrawUI(jumpUISprite, jumpTex, Sprite::AnchorPoint::BottomLeft, BlendMode::kBlendModeNormal);
   }

   if (cameraUISprite) {
	  EngineContext::DrawUI(cameraUISprite, cameraTex, Sprite::AnchorPoint::BottomLeft, BlendMode::kBlendModeNormal);
   }

   if (spinUISprite) {
	  EngineContext::DrawUI(spinUISprite, spinTex, Sprite::AnchorPoint::BottomLeft, BlendMode::kBlendModeNormal);
   }
}

void GameSceneRenderer::DrawGameObjects(
   Model* playerModel,
   Model* starModel,
   const std::vector<std::unique_ptr<Rabbit>>& rabbits,
   const std::vector<std::unique_ptr<Planet>>& planets,
   const std::vector<std::unique_ptr<class Box>>& boxes,
   bool showPlayer
) {
   auto planetTex = EngineContext::GetTexture("planet");
   auto playerTex = EngineContext::GetTexture("player");
   auto starTex = EngineContext::GetTexture("white1x1");
   auto rabbitTex = EngineContext::GetTexture("rabbit");
   auto boxTex = EngineContext::GetTexture("white1x1");

   // メインビジュアルの描画
   EngineContext::SetActiveCamera(1);
   EngineContext::SetBlendMode(BlendMode::kBlendModeNone);

   // プレイヤーの描画（演出中は非表示）
   if (playerModel && showPlayer) {
	  if (playerTex) EngineContext::Draw(playerModel, playerTex);
	  else EngineContext::Draw(playerModel, std::vector<Texture*>{});
   }

   // スターの描画
   if (starModel) {
	  if (starTex) EngineContext::Draw(starModel, starTex);
	  else EngineContext::Draw(starModel, std::vector<Texture*>{});
   }

   // 全ラビットの描画
   for (auto& rabbit : rabbits) {
	  if (rabbit && rabbit->GetModel()) {
		 if (rabbitTex) EngineContext::Draw(rabbit->GetModel(), rabbitTex);
		 else EngineContext::Draw(rabbit->GetModel(), std::vector<Texture*>{});
	  }
   }
   
   // 全ボックスの描画
   for (const auto& box : boxes) {
	  if (box && box->GetModel()) {
		 if (boxTex) EngineContext::Draw(box->GetModel(), boxTex);
		 else EngineContext::Draw(box->GetModel(), std::vector<Texture*>{});
	  }
   }

   // 全惑星の描画
   for (auto& planet : planets) {
	  if (planet && planet->GetModel()) {
		 if (planetTex) {
			EngineContext::Draw(planet->GetModel(), planetTex);
		 }
	  }
   }
}

void GameSceneRenderer::DrawParticles(
   ParticleSystem* particleSmoke,
   ParticleSystem* particleStar,
   ParticleSystem* particleDust,
   ParticleSystem* particleStarCollection,
   ParticleSystem* particleRabbitCapture,
   bool showPlayerParticles
) {
   // プレイヤーの移動パーティクル（煙）- 演出中は非表示
   if (particleSmoke && showPlayerParticles) {
	  EngineContext::Draw(particleSmoke);
   }

   if (particleStar) {
	  EngineContext::Draw(particleStar);
   }

   if (particleDust) {
	  EngineContext::Draw(particleDust);
   }

   if (particleStarCollection) {
	  EngineContext::Draw(particleStarCollection);
   }

   if (particleRabbitCapture) {
	  EngineContext::Draw(particleRabbitCapture);
   }
}

void GameSceneRenderer::DrawDebug(LevelEditor* levelEditor) {
#ifdef USE_IMGUI
   // レベルエディターのデバッグ描画
   if (levelEditor) {
	  levelEditor->Draw();
   }
#endif
}
