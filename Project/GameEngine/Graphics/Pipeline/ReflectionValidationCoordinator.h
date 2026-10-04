#pragma once
#include "GameEngine/Graphics/Pipeline/ReflectionValidationState.h"

namespace GameEngine {

class PSOManager;
class ShaderManager;

/// @brief シェーダーリフレクション検証のフレーム処理とレポート更新を調停する
class ReflectionValidationCoordinator {
public:
   /// @brief フレーム開始時に使用シェーダーの収集状態をリセットする
   /// @param shaderManager シェーダーマネージャー
   /// @param state 反射検証状態
   void BeginFrame(ShaderManager* shaderManager, ReflectionValidationState& state) const;

   /// @brief フレーム中に収集した情報を確定して検証状態へ反映する
   /// @param shaderManager シェーダーマネージャー
   /// @param state 反射検証状態
   void EndFrame(ShaderManager* shaderManager, ReflectionValidationState& state) const;

   /// @brief PSOとシェーダーの対応を検証してレポートを更新する
   /// @param psoManager パイプラインステートマネージャー
   /// @param shaderManager シェーダーマネージャー
   /// @param state 更新する反射検証状態
   void UpdateValidationReport(const PSOManager* psoManager, ShaderManager* shaderManager, ReflectionValidationState& state) const;

#ifdef USE_IMGUI
   /// @brief 検証結果と再検証操作を提供するデバッグウィンドウを描画する
   /// @param psoManager パイプラインステートマネージャー
   /// @param shaderManager シェーダーマネージャー
   /// @param state 反射検証状態
   void DrawDebugWindow(const PSOManager* psoManager, ShaderManager* shaderManager, ReflectionValidationState& state) const;
#endif
};

}
