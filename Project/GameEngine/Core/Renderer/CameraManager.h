#pragma once
#include <vector>

namespace GameEngine {
class Camera;

/// @brief カメラマネージャークラス
class CameraManager {
public:
   /// @brief カメラを追加
   /// @param camera 追加するカメラへのポインタ
   void AddCamera(Camera* camera);

   /// @brief アクティブなカメラを設定
   /// @param index 設定するカメラのインデックス（デフォルトは0）
   void SetActiveCamera(size_t index = 0);

   /// @brief アクティブなカメラを取得
   /// @return アクティブなカメラへのポインタ
   Camera* GetActiveCamera() const { return activeCamera_; }

   /// @brief 指定したインデックスのカメラを取得
   /// @param index 取得するカメラのインデックス
   Camera* GetCamera(size_t index) const { return (index < cameras_.size()) ? cameras_[index] : nullptr; }

   /// @brief カメラの数を取得
   /// @return カメラの数
   size_t GetCameraCount() const { return cameras_.size(); }

   /// @brief すべてのカメラをクリア
   void ClearCameras() { cameras_.clear(); activeCamera_ = nullptr; }

   /// @brief すべてのカメラを取得
   /// @return カメラのベクターへの参照
   const std::vector<Camera*>& GetCameras() const { return cameras_; }
private:
   std::vector<Camera*> cameras_;
   Camera* activeCamera_ = nullptr;
};
}
