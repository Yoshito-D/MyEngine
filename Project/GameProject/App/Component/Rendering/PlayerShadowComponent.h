#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"

namespace GameEngine {
class Camera;
struct PlayerShadowFrameData;
}

namespace App {
/// @brief 車両の着地先への影描画を準備する。操作制御とは独立して有効化する。
class PlayerShadowComponent final : public GameEngine::IObjectComponent {
public:
   /// @brief シーンで使用する型名。
   static constexpr const char* kTypeName = "PlayerShadowComponent";
   /// @brief エディターに表示する名前。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"プレイヤー影", "Player Shadow"};

   /// @brief 保存用の型名を返す。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 車両停止中も、着地の目印になる影を現在フレームの描画へ提出する。
   /// @param context 通常シーンの描画カメラ。
   void PrepareDraw(const GameEngine::SceneDrawContext& context) override;

   /// @brief 独自の保存設定はないため空のオブジェクトを返す。
   nlohmann::json Serialize() const override { return nlohmann::json::object(); }

   /// @brief 独自の保存設定を持たないため読み込み処理は行わない。
   void Deserialize(const nlohmann::json&) override {}

private:
   /// @brief オーナーの車両モデルと着地候補から影の入力データを組み立てる。
   /// @param camera 通常描画のカメラ。nullptrなら失敗。
   /// @param outFrameData モデル・カメラとワールド座標を格納する出力。失敗時は空に戻す。
   /// @return 有効な車両モデルと惑星モデル、正の半径を取得できた場合true。
   /// @note PlanetSwitcherの3引数版で同じ候補のモデル・中心・半径を取得する。
   /// 位置は親変換を含むワールド座標にそろえ、入力収集の段階ではGPU定数を書き換えない。
   bool TryBuildPlayerShadowFrameData(GameEngine::Camera* camera, GameEngine::PlayerShadowFrameData& outFrameData);
};
} // namespace App
