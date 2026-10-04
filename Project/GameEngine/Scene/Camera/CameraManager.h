#pragma once
#include <vector>
#include <memory>

namespace GameEngine {
class Camera;
class CinemachineBrain;

/// @brief カメラマネージャークラス
/// CameraUnit（Brain+Camera）を複数管理しRendererへ出力カメラを提供する
class CameraManager {
public:
   CameraManager();
   ~CameraManager();

   /// @brief CameraUnitを生成して追加する（最初の生成時にアクティブになる）
   /// @return 初期化済みBrain。所有権はCameraManagerが持つ。
   CinemachineBrain* CreateUnit(std::unique_ptr<Camera> outputCamera);

   /// @brief アクティブなBrainを取得（shortcut）
   CinemachineBrain* GetActiveBrain();
   /// @brief const Managerからはアクティブな出力を読み取り専用で観測する。
   const CinemachineBrain* GetActiveBrain() const;

   /// @brief アクティブなカメラを取得（Renderer用）
   Camera* GetActiveCamera();
   /// @brief const Managerからはアクティブな出力を読み取り専用で観測する。
   const Camera* GetActiveCamera() const;

   /// @brief 全CameraUnitを削除しアクティブをリセット
   void ClearUnits();

private:
   struct CameraUnit { std::unique_ptr<CinemachineBrain> brain; };
   std::vector<std::unique_ptr<CameraUnit>> units_;
   CameraUnit* activeUnit_ = nullptr;
};
}
