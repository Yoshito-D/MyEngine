#pragma once

#include "GameProject/App/Component/Race/StageIntroSequence.h"
#include "GameEngine/Scene/Camera/Core/CinemachineBrain.h"
#include <string>
#include <vector>

namespace GameEngine {
class SceneWorld;
}

namespace App {
class VehicleController;
class VehicleInputComponent;
class GravityBody;
class CameraModeSwitcher;
class VehicleSpeedPostEffectController;

/// @brief レースの各状態で必要なプレイヤー停止とカメラ・速度演出を適用する。
class RacePresentation {
public:
   /// @brief 指定されたプレイヤーから操作と演出の参照を解決する。
   void Resolve(GameEngine::SceneWorld& sceneWorld, const std::string& playerObjectId);

   /// @brief シーン本来の有効状態を保存し、一時停止からの復元に使う。
   void CaptureInitialState();

   /// @brief 入力・走行・物理を一時停止するか、保存した有効状態へ戻す。
   void SetPlayerLocked(bool locked);

   /// @brief 現在レース演出による操作停止中かを返す。
   bool IsPlayerLocked() const { return playerLocked_; }

   /// @brief 速度演出とカメラ切り替えをシーン本来の状態へ戻す。
   void RestoreEffects();

   /// @brief 専用カメラと通常カメラを確認し、操作を停止して俯瞰導入を開始する。
   /// @return 設定と参照が有効で、再生を開始した場合はtrue。
   bool BeginIntro(GameEngine::CinemachineBrain* brain, const StageIntroSettings& settings);

   /// @brief 導入を更新し、黒画面内で通常カメラへ戻す。参照消失時は中止して完了を返す。
   StageIntroSequence::Frame UpdateIntro(
      GameEngine::CinemachineBrain* brain, float deltaTime, bool skipRequested, bool sceneTransitioning);

   /// @brief 専用カメラを解除し、生存している対象の操作・演出・惑星色更新を復元する。
   void CancelIntro(GameEngine::CinemachineBrain* brain);

   /// @brief 速度演出を止め、指定されたゴールカメラへ固定する。
   void ShowFinish(const std::string& finishCameraId);

   /// @brief 指定Brainの追従を停止・再開し、再開時の補間を設定する。
   static void SetCameraMotionPaused(GameEngine::CinemachineBrain* brain, bool paused, float blendSeconds,
      GameEngine::CinemachineBrain::BlendEasing easing);

   /// @brief カウントダウン中の位置・姿勢・画角を選択カメラへ適用する。
   void ApplyCountdownCameraPose(GameEngine::CinemachineBrain* brain, const GameEngine::Vector3& position,
      const GameEngine::Vector3& rotation, float fov);

private:
   void RefreshPlayerReferences();
   void RestoreIntroCamera(GameEngine::CinemachineBrain* brain);
   void PausePlanetColorUpdates();
   void RestorePlanetColorUpdates();

   GameEngine::SceneWorld* world_ = nullptr;             ///< 非所有参照。対象の生存確認に使用する。
   std::string playerObjectId_;                          ///< プレイヤー削除後の古い参照を使わないための安定ID。
   StageIntroSequence introSequence_;                    ///< 俯瞰移動と暗転の再生器。
   std::string introCameraId_;                           ///< 再生中だけ使用する専用カメラID。
   std::string countdownCameraId_;                       ///< 導入開始前に選択されていた通常カメラID。
   GameEngine::CameraState countdownCameraState_;        ///< 通常カメラへ戻すときの開始前の構図。
   bool introActive_ = false;                            ///< 導入による一時停止を所有しているか。
   std::vector<std::string> pausedPlanetColorObjectIds_; ///< 導入中に停止した惑星色コンポーネントの所有者ID。
   bool playerLocked_ = false;                           ///< レース進行によるプレイヤーの一時停止が適用中かどうか。
   bool vehicleControllerEnabledWhenUnlocked_ = false;   ///< 操作ロック解除時に戻す車両制御の元の有効状態。
   bool vehicleInputEnabledWhenUnlocked_ = false;        ///< 操作ロック解除時に戻す走行入力の元の有効状態。
   bool gravityBodyEnabledWhenUnlocked_ = false;         ///< 操作ロック解除時に戻す重力処理の元の有効状態。
   bool cameraSwitcherEnabledWhenUnlocked_ = false;      ///< 操作ロック解除時に戻すカメラ切り替えの元の有効状態。
   bool speedPostEffectEnabledWhenRacing_ = false;       ///< レース再開時に戻す速度演出制御の元の有効状態。
   /// @brief プレイヤーの車両操作を制御するコンポーネントへの非所有参照。
   VehicleController* vehicleController_ = nullptr;
   VehicleInputComponent* vehicleInput_ = nullptr; ///< カメラ演出にも共有される走行用入力への非所有参照。
   GravityBody* gravityBody_ = nullptr;            ///< プレイヤーの速度・重力・姿勢を参照するコンポーネント。
   CameraModeSwitcher* cameraSwitcher_ = nullptr;  ///< プレイヤーの仮想カメラ切り替えを行うコンポーネントへの参照。
   /// @brief プレイヤー速度に応じた画面演出を制御するコンポーネントへの参照。
   VehicleSpeedPostEffectController* speedPostEffectController_ = nullptr;
};
}
