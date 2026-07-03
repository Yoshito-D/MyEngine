#pragma once
#include "ReflectionValidationState.h"

namespace GameEngine {

class PSOManager;
class ShaderManager;

class ReflectionValidationCoordinator {
public:
   // @brief フレーム開始時の処理
   // @param shaderManager シェーダーマネージャー
   // @param state 反射検証状態
   void BeginFrame(ShaderManager* shaderManager, ReflectionValidationState& state) const;

   // @brief フレーム終了時の処理
   // @param shaderManager シェーダーマネージャー
   // @param state 反射検証状態
   void EndFrame(ShaderManager* shaderManager, ReflectionValidationState& state) const;

   // @brief 反射検証レポートを更新
   // @param psoManager パイプラインステートマネージャー
   // @param shaderManager シェーダーマネージャー
   void UpdateValidationReport(PSOManager* psoManager, ShaderManager* shaderManager, ReflectionValidationState& state) const;

#ifdef USE_IMGUI
   // @brief デバッグウィンドウを描画
   // @param psoManager パイプラインステートマネージャー
   // @param shaderManager シェーダーマネージャー
   // @param state 反射検証状態
   void DrawDebugWindow(PSOManager* psoManager, ShaderManager* shaderManager, ReflectionValidationState& state) const;
#endif
};

}
