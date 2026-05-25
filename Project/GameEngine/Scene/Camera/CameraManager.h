#pragma once
#include <vector>
#include <memory>

namespace GameEngine {
class Camera;
class CinemachineBrain;

/// @brief CinemachineBrainとCameraのペアを表す単位
struct CameraUnit {
    std::unique_ptr<CinemachineBrain> brain;
};

/// @brief カメラマネージャークラス
/// CameraUnit（Brain+Camera）を複数管理しRendererへ出力カメラを提供する
class CameraManager {
public:
   CameraManager();
   ~CameraManager();

   /// @brief CameraUnitを生成して追加する（最初の生成時にアクティブになる）
   /// @return 生成したCameraUnit（所有権はCameraManagerが持つ）
   CameraUnit* CreateUnit();

   /// @brief アクティブなCameraUnitを取得
   CameraUnit* GetActiveUnit() const { return activeUnit_; }

   /// @brief アクティブなBrainを取得（shortcut）
   CinemachineBrain* GetActiveBrain() const;

   /// @brief アクティブなカメラを取得（Renderer用）
   Camera* GetActiveCamera() const;

   /// @brief 全CameraUnitを削除しアクティブをリセット
   void ClearUnits();

private:
   std::vector<std::unique_ptr<CameraUnit>> units_;
   CameraUnit* activeUnit_ = nullptr;
};
}
