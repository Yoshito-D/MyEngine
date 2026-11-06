#pragma once

#include "GameEngine/Math/VectorMath.h"
#include <memory>
#include <string>
#include <vector>

namespace GameEngine {

class SceneWorld;
class ParticleSystem;

/// @brief 1ワールドの子エフェクト生成要求と生成後の寿命を所有する。
class ParticleSubEmitterRuntime final {
public:
   /// @brief 子エフェクトを生成するワールドを関連付ける。
   explicit ParticleSubEmitterRuntime(SceneWorld& world);

   /// @brief 残存する子エフェクトを破棄する。
   ~ParticleSubEmitterRuntime();

   /// @brief ワールド更新後に生成する子エフェクトを予約する。
   /// @param position 子エフェクトの原点に加えるワールド位置（m）。
   void Queue(const std::string& effectPath, const Vector3& position);

   /// @brief 完了した子を解除し、予約分を再帰せずに実体化する。
   void ProcessPending();

   /// @brief 未処理要求を捨て、所有する子を更新・描画一覧から解除する。
   void Clear();

   /// @brief 描画終了後に解除済みの子とGPU資源を解放する。
   void FinishFrame();

private:
   struct PendingEvent {
      std::string effectPath; ///< 子エフェクト設定のパス。
      Vector3 position;       ///< ワールド位置（m）。
   };

   void Retire(std::unique_ptr<ParticleSystem> child);

   SceneWorld& world_;                                            ///< 所有ワールド。実行系より長く生存する。
   std::vector<PendingEvent> pendingEvents_;                      ///< 更新終了後に処理する所有要求。
   std::vector<std::unique_ptr<ParticleSystem>> children_;        ///< 生成した子の排他的所有一覧。
   std::vector<std::unique_ptr<ParticleSystem>> retiredChildren_; ///< 描画終了までGPU資源を保持する解除済みの子。
};

} // namespace GameEngine
