#pragma once
#include <unordered_map>
#include <string>
#include <optional>
#include <functional>
#include <nlohmann/json.hpp>
#include "Core/Input/Input.h"
#include "Framework/EngineContext.h"

namespace GameEngine {
/// @brief アクション名と入力を紐付けるキーコンフィグ管理クラス
class KeyConfig {
public:
   struct ActionCallbacks {
	  std::function<void()> onPressed{};   // 押されている間（毎フレーム）
	  std::function<void()> onTriggered{}; // 押された瞬間（1フレーム）
	  std::function<void()> onReleased{};  // 離された瞬間（1フレーム）
   };

   /// @brief コンストラクタ
   /// @param filePath デフォルトの保存/読込パス
   explicit KeyConfig(std::string filePath = "config/keybindings.json");

   // バインド操作 -----------------------------------------------------
   void BindKey(const std::string& action, KeyCode key);
   void BindMouse(const std::string& action, MouseButton button);
   void BindGamePad(const std::string& action, GamePadButton button);

   /// @brief 左スティックをアクションにバインド
   void BindLeftStick(const std::string& action);
   /// @brief 右スティックをアクションにバインド
   void BindRightStick(const std::string& action);

   void Unbind(const std::string& action);
   void Clear();

   // コールバック登録 -------------------------------------------------
   void RegisterCallbacks(const std::string& action, const ActionCallbacks& callbacks);
   void RegisterOnPressed(const std::string& action, std::function<void()> cb);
   void RegisterOnTriggered(const std::string& action, std::function<void()> cb);
   void RegisterOnReleased(const std::string& action, std::function<void()> cb);
   void RemoveCallbacks(const std::string& action);

   // 問い合わせ -------------------------------------------------------
   std::optional<KeyCode> GetKeyBinding(const std::string& action) const;
   std::optional<MouseButton> GetMouseBinding(const std::string& action) const;
   std::optional<GamePadButton> GetGamePadBinding(const std::string& action) const;

   /// @brief スティックのバインディングを取得 (0=なし, 1=左, 2=右)
   int GetStickBinding(const std::string& action) const;

   /// @brief アクションが押されているか（EngineContext経由）
   bool IsPressed(const std::string& action, uint32_t gamePadIndex = 0) const;
   /// @brief アクションがトリガー（押された瞬間）されたか（EngineContext経由）
   bool IsTriggered(const std::string& action, uint32_t gamePadIndex = 0) const;
   /// @brief アクションがリリース（離れた瞬間）されたか（EngineContext経由）
   bool IsReleased(const std::string& action, uint32_t gamePadIndex = 0) const;

   /// @brief スティック入力をVector2として取得
   /// @param action アクション名
   /// @param gamePadIndex ゲームパッド番号（通常0）
   /// @param deadZone デッドゾーンの値（0.0～1.0）
   /// @return スティックの入力ベクトル（バインドされていない場合は(0,0)）
   Vector2 GetStickVector(const std::string& action, uint32_t gamePadIndex = 0, float deadZone = 0.24f) const;

   /// @brief スティック入力がしきい値を超えているか（bool判定用）
   /// @param action アクション名
   /// @param gamePadIndex ゲームパッド番号（通常0）
   /// @param threshold しきい値（0.0～1.0、デフォルト0.5）
   /// @return スティックがしきい値を超えていればtrue
   bool IsStickActive(const std::string& action, uint32_t gamePadIndex = 0, float threshold = 0.5f) const;

   /// @brief すべての登録アクションの入力を評価してコールバックを実行
   void Update(uint32_t gamePadIndex = 0) const;

   // 永続化 -----------------------------------------------------------
   bool Load(const std::string& filePath = "");
   bool Save(const std::string& filePath = "") const;

   /// @brief デフォルト設定を投入（必要に応じて呼び出し）
   void SetDefaultBindings();

   /// @brief 現在の保存/読込パスを取得
   const std::string& GetFilePath() const { return filePath_; }
   /// @brief 保存/読込パスを設定
   void SetFilePath(const std::string& path) { filePath_ = path; }

private:
   // JSON シリアライズ/デシリアライズ
   nlohmann::json ToJson() const;
   void FromJson(const nlohmann::json& j);

private:
   std::unordered_map<std::string, KeyCode> keyBindings_;
   std::unordered_map<std::string, MouseButton> mouseBindings_;
   std::unordered_map<std::string, GamePadButton> gamePadBindings_;
   std::unordered_map<std::string, int> stickBindings_; // 0=なし, 1=左スティック, 2=右スティック
   std::unordered_map<std::string, ActionCallbacks> callbacks_;
   std::string filePath_;
};
}
