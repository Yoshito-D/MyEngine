#pragma once
#include "GameEngine/Scene/Camera/Core/CinemachineBrain.h"
#include <string>

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
   /// @brief 速度演出を止め、指定されたゴールカメラへ固定する。
   void ShowFinish(const std::string& finishCameraId);
   /// @brief 指定Brainの追従を停止・再開し、再開時の補間を設定する。
   static void SetCameraMotionPaused(GameEngine::CinemachineBrain* brain, bool paused, float blendSeconds,
      GameEngine::CinemachineBrain::BlendEasing easing);
   /// @brief カウントダウン中の位置・姿勢・画角を選択カメラへ適用する。
   void ApplyCountdownCameraPose(GameEngine::CinemachineBrain* brain, const GameEngine::Vector3& position,
      const GameEngine::Vector3& rotation, float fov);

private:
   bool playerLocked_ = false;                         ///< レース進行によるプレイヤーの一時停止が適用中かどうか。
   bool vehicleControllerEnabledWhenUnlocked_ = false; ///< 操作ロック解除時に戻す車両制御の元の有効状態。
   bool vehicleInputEnabledWhenUnlocked_ = false;      ///< 操作ロック解除時に戻す走行入力の元の有効状態。
   bool gravityBodyEnabledWhenUnlocked_ = false;       ///< 操作ロック解除時に戻す重力処理の元の有効状態。
   bool cameraSwitcherEnabledWhenUnlocked_ = false;    ///< 操作ロック解除時に戻すカメラ切り替えの元の有効状態。
   bool speedPostEffectEnabledWhenRacing_ = false;     ///< レース再開時に戻す速度演出制御の元の有効状態。
   /// @brief プレイヤーの車両操作を制御するコンポーネントへの非所有参照。
   VehicleController* vehicleController_ = nullptr;
   VehicleInputComponent* vehicleInput_ = nullptr; ///< カメラ演出にも共有される走行用入力への非所有参照。
   GravityBody* gravityBody_ = nullptr;            ///< プレイヤーの速度・重力・姿勢を参照するコンポーネント。
   CameraModeSwitcher* cameraSwitcher_ = nullptr;  ///< プレイヤーの仮想カメラ切り替えを行うコンポーネントへの参照。
   /// @brief プレイヤー速度に応じた画面演出を制御するコンポーネントへの参照。
   VehicleSpeedPostEffectController* speedPostEffectController_ = nullptr;
};
}
