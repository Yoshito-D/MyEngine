#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/Types/Vector3.h"
#include "GameProject/App/Component/Gravity/GravityBody.h"
#include "GameProject/App/Component/Character/CharacterJump.h"
#include "GameProject/App/Component/Camera/GravityFollowCamera.h"
#include "GameProject/App/Component/Vehicle/VehicleInputComponent.h"
#include "GameProject/App/Component/Vehicle/VehicleMover.h"

namespace GameEngine {
class Camera;
struct PlayerShadowFrameData;
}

namespace App {

/// @brief 入力を収集して車の移動・ジャンプへ振り分けるコンポーネント
class VehicleController final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "VehicleController";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "車両制御", "Vehicle Controller" };

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 入力取得と各サブコンポーネントへの委譲を行う
   void Update(float deltaTime) override;

   /// @brief GravityFollowCamera 参照を設定する
   void SetGravityFollowCamera(GravityFollowCamera* cam);

   /// @brief 直近の移動方向を取得する
   GameEngine::Vector3 GetLastMoveDirection() const;

   /// @brief 車両制御が無効でも、この車両の着地目印となる影を今フレームのRendererへ送る。
   /// @param camera 通常シーンの描画に使うカメラ
   /// @note 物理・惑星選択・アニメーション・カメラ更新後、EndFrame前の描画処理から呼ぶ。
   /// 実装時はTryBuildPlayerShadowFrameDataで対象を集め、通常描画を省くモデルも行列を更新してから
   /// EngineContextへ渡す。対象が無効ならClearPlayerShadowFrameDataで以前の送信を取り消す。
   void SubmitPlayerShadow(GameEngine::Camera* camera);

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief パラメータをシリアライズする
   nlohmann::json Serialize() const override;

   /// @brief パラメータをデシリアライズする
   void Deserialize(const nlohmann::json& data) override;

private:
   /// @brief オーナーの車両モデルと着地候補から影の入力データを組み立てる。
   /// @param camera 通常描画のカメラ。nullptrなら失敗。
   /// @param outFrameData モデル・カメラとワールド座標を格納する出力。失敗時は空に戻す。
   /// @return 有効な車両モデルと惑星モデル、正の半径を取得できた場合true。
   /// @note PlanetSwitcherの3引数版で同じ候補のモデル・中心・半径を取得する。
   /// 位置は親変換を含むワールド座標にそろえ、入力収集の段階ではGPU定数を書き換えない。
   bool TryBuildPlayerShadowFrameData(GameEngine::Camera* camera,
      GameEngine::PlayerShadowFrameData& outFrameData);

   /// @brief 選択中カメラの安定ID。削除されたComponentの生ポインターを保持しない
   std::string gravityFollowCameraId_;
};

} // namespace App
