#pragma once

#include "GameEngine/Input/Input.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include <nlohmann/json.hpp>

namespace GameEngine {

/// @brief スティック入力からアクションへ渡す成分。
enum class InputBindingAxis {
   Both,
   X,
   Y
};

/// @brief 単一キーの割り当て。
struct KeyInputBinding {
   KeyCode key = KeyCode::Space; ///< 押下状態を読むキー。
};

/// @brief 単一ゲームパッドボタンの割り当て。
struct GamePadButtonInputBinding {
   GamePadButton button = GamePadButton::A; ///< 押下状態を読むボタン。
};

/// @brief 二つのキーを負・正方向へ割り当てる一次元軸。
struct KeyAxis1DInputBinding {
   KeyCode negative = KeyCode::A; ///< 負方向のキー。
   KeyCode positive = KeyCode::D; ///< 正方向のキー。
};

/// @brief 四つのキーを左右上下へ割り当てる二次元軸。
struct KeyAxis2DInputBinding {
   KeyCode left = KeyCode::Left;   ///< 左方向のキー。
   KeyCode right = KeyCode::Right; ///< 右方向のキー。
   KeyCode up = KeyCode::Up;       ///< 上方向のキー。
   KeyCode down = KeyCode::Down;   ///< 下方向のキー。
};

/// @brief 左スティックの割り当て。
struct LeftStickInputBinding {
   InputBindingAxis axis = InputBindingAxis::Both; ///< アクションへ渡す成分。
};

/// @brief 右スティックの割り当て。
struct RightStickInputBinding {
   InputBindingAxis axis = InputBindingAxis::Both; ///< アクションへ渡す成分。
};

/// @brief 競合判定と再割り当てに使う単一の物理入力。
struct InputPhysicalControl {
   /// @brief 物理入力が属するデバイス上の種類。
   enum class Type {
      Key,
      GamePadButton,
      LeftStick,
      RightStick
   };

   Type type = Type::Key;   ///< キー・ボタン・左右スティックの種類。
   std::uint32_t value = 0; ///< キーまたはボタンの識別値。スティックは0。

   /// @brief 種類と識別値が一致するか比較する。
   bool operator==(const InputPhysicalControl&) const = default;
};

/// @brief 再割り当てUIへ表示する、バインド全体または複合軸の一方向。
struct InputBindingEditPart {
   const char* label;                  ///< UIへ表示する操作名。
   std::optional<size_t> controlIndex; ///< 未設定ならバインド全体、設定時は物理入力列の添字。
};

/// @brief 種類に必要なデータだけを保持し、型別の操作へ委譲する入力バインド。
struct InputBinding {
   /// @brief 有効なバインド形式のいずれか一つ。
   using Source = std::variant<KeyInputBinding, GamePadButtonInputBinding, KeyAxis1DInputBinding, KeyAxis2DInputBinding,
      LeftStickInputBinding, RightStickInputBinding>;

   Source source = KeyInputBinding{}; ///< 現在の割り当て形式と、その形式だけが持つ設定。
   float scale = 1.0f;                ///< アクションへ合成する前に掛ける倍率。

   /// @brief 物理入力を評価する。キーボードはプレイヤー0だけに割り当てる。
   Vector2 Evaluate(IInputBackend& backend, std::uint32_t playerSlot, float deadZone) const;

   /// @brief 現在の形式に必要な項目だけを保存する。
   nlohmann::json Serialize() const;

   /// @brief JSON全体を検証して復元する。不正形式・未登録入力・非有限倍率なら未設定を返す。
   static std::optional<InputBinding> Deserialize(const nlohmann::json& data);

   /// @brief 種類と割り当て内容を表示用文字列へ変換する。
   std::string Describe() const;

   /// @brief 倍率と物理入力・軸成分が有効か調べる。
   bool IsValid() const;

   /// @brief 複合軸を含め、物理入力を安定した方向順へ展開する。
   std::vector<InputPhysicalControl> GetPhysicalControls() const;

   /// @brief UIから再割り当てできる全体・方向を表示順で返す。
   std::vector<InputBindingEditPart> GetEditParts() const;

   /// @brief 指定された方向の物理入力を置換する。種類や添字が合わなければ変更しない。
   bool SetPhysicalControl(size_t index, const InputPhysicalControl& control);

   /// @brief 同じデバイス種別の物理入力を全て置換する。該当しない場合はfalse。
   bool ReplacePhysicalControl(const InputPhysicalControl& target, const InputPhysicalControl& replacement);

   /// @brief 他のバインドと一つ以上の物理入力を共有しているか調べる。
   bool SharesPhysicalControl(const InputBinding& other) const;

   /// @brief 指定した物理入力を含むか調べる。
   bool ContainsPhysicalControl(const InputPhysicalControl& control) const;

   /// @brief 一回の入力捕捉を、全体または複合軸の指定方向へ適用したコピーを返す。
   /// @details 倍率は維持する。方向指定にはキーだけを許可し、全体指定ではキーとボタンを許可する。
   std::optional<InputBinding> Rebind(const InputPhysicalControl& control, std::optional<size_t> controlIndex) const;
};

/// @brief 再割り当てに利用できるキーまたはプレイヤー0のボタンの、新しい押下を一つ捕捉する。
/// @param allowGamePad falseならキーだけを捕捉する。
std::optional<InputPhysicalControl> CaptureRebindControl(IInputBackend& backend, bool allowGamePad);

} // namespace GameEngine
