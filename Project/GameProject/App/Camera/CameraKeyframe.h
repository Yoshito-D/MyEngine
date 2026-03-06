#pragma once
#include "Utility/VectorMath.h"
#include "Utility/MathUtils/EasingFunctions.h"
#include <functional>

/// @brief カメラのキーフレーム（特定の時点でのカメラの状態）
struct CameraKeyframe {
   /// @brief イージングタイプ
   enum class EasingType {
	  Linear,        // 線形補間
	  EaseIn,        // イーズイン
	  EaseOut,       // イーズアウト
	  EaseInOut,     // イーズインアウト
	  Bounce,        // バウンス
	  EaseOutBack    // バックイージング（行き過ぎ）
   };

   GameEngine::Vector3 position;           // カメラ位置
   GameEngine::Vector3 rotation;           // カメラ回転（Euler角）
   float fov = 0.45f;          // 視野角
   float duration = 1.0f;      // このキーフレームまでの時間（秒）
   EasingType easingType = EasingType::Linear; // イージングタイプ
   float easingPower = 2.0f;   // イージングの強さ（EaseIn/Out/InOutで使用）
   bool useFade = false;       // フェードを使用するか
   float fadeDuration = 0.5f;  // フェード時間（秒）
   uint32_t fadeColor = 0x000000ff; // フェードカラー

   /// @brief 2つのキーフレーム間を補間する
   /// @param start 開始キーフレーム
   /// @param end 終了キーフレーム
   /// @param t 補間係数（0.0～1.0）
   /// @return 補間されたキーフレーム
   static CameraKeyframe Interpolate(const CameraKeyframe& start, const CameraKeyframe& end, float t);

   /// @brief イージング関数を適用
   /// @param t 補間係数（0.0～1.0）
   /// @param type イージングタイプ
   /// @param power イージングの強さ
   /// @return イージング適用後の値
   static float ApplyEasing(float t, EasingType type, float power);
};
