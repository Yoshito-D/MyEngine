#pragma once

#include "GameEngine/UI/UILayoutSolver.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace GameEngine {
class Object;
class SceneWorld;
class UIWidgetComponent;

/// @brief デバイスとエディターから切り離した一フレーム分のUI入力。
struct UIInputFrame {
   Vector2 pointer{}; ///< 描画先に正規化したUIピクセル。
   bool active = true;
   bool gamepadConnected = false; ///< ボタンの押下状態とは独立した、ゲームパッドの接続状態。
   bool pointerInside = false;
   bool pointerMoved = false;
   bool pointerDown = false;
   bool pointerPressed = false;
   bool pointerReleased = false;
   bool confirm = false;
   bool cancel = false;
   bool next = false;
   bool previous = false;
   float vertical = 0.0f;
   float horizontal = 0.0f;
};

/// @brief シーン所有の画面スタック・フォーカス・入力配送。変更はフレーム開始と配送終了後に確定する。
class UISystem final {
public:
   /// @brief 所有元ワールドへの非所有参照を接続する。
   explicit UISystem(SceneWorld& world);

   /// @brief 所有元が寿命を管理するUI実行系を破棄する。
   ~UISystem();

   /// @brief アセットから画面を生成してIDを返す。表示はPushで要求する。
   std::string InstantiateScreen(const std::string& assetId);

   /// @brief 既存画面の表示を要求する。受付後のフレーム開始または入力配送終了で確定する。
   bool Push(const std::string& rootId);

   /// @brief 最前面画面の終了演出を要求する。演出完了は次のフレーム開始で反映する。
   void Pop();

   /// @brief 画面とコールバックの解除を要求する。Objectと配置順は保持する。
   void Clear();

   /// @brief シーン破棄前に画面状態・要求・配置順を即時解除する。入力配送中には呼び出さない。
   void ResetForScene();

   /// @brief 画面と子孫の削除を要求する。入力配送中に実体や配置結果を変更しない。
   void RemoveScreen(const std::string& rootId);

   /// @brief シーン初期化後、自動表示する画面のPushを要求する。
   void InitializeScreens();

   /// @brief 開閉完了と要求を確定し、入力を配送して、このフレームの停止方針を固定する。
   /// @param viewportSize 描画先のUIピクセル寸法。
   /// @param unscaledDeltaTime UI方向入力のリピートに使用する秒数。Componentはここで更新しない。
   /// @param runtimeUpdate falseでは入力と終了演出の完了反映を停止し、削除済み参照だけを解除する。
   void BeginFrame(Vector2 viewportSize, const UIInputFrame& input, float unscaledDeltaTime, bool runtimeUpdate = true);

   /// @brief 独立した配置結果を返す。描画・編集から画面状態を変更しない。
   std::vector<UIElementLayout> BuildLayout(Vector2 viewportSize, bool editing = false) const;

   /// @brief 今フレームに確定したゲーム停止方針を返す。
   bool PausesGameplay() const { return gameplayPaused_; }

   /// @brief 今フレームに確定したゲーム入力遮断方針を返す。
   bool BlocksGameplayInput() const { return gameplayInputBlocked_; }

   /// @brief CanvasまたはScreen配下の要素かを返す。閉じた画面も含める。
   bool IsManaged(const Object& object) const;

   /// @brief 実行画面順と親の表示設定からUI更新の可否を判定する。編集レイアウトは参照しない。
   bool IsUpdateVisible(const Object& object) const;

   /// @brief 画面内の一意なキーからObjectを検索する。重複キーの場合はnullptr。
   Object* FindElement(const std::string& rootId, const std::string& elementKey) const;

   /// @brief 指定画面で現在選択されている操作要素のIDを返す。未表示・終了中・選択なしでは空文字列。
   std::string GetFocusedElementId(const std::string& rootId) const;

   /// @brief 保存と配置計算で共有する階層順を設定する。
   void SetHierarchyOrder(const std::vector<std::string>& order) { hierarchyOrder_ = order; }

   /// @brief 保存用の階層順を読み取り専用で返す。
   const std::vector<std::string>& GetHierarchyOrder() const { return hierarchyOrder_; }

   /// @brief 直近の画面アセット生成の失敗理由を返す。
   const std::string& GetLastError() const { return lastError_; }

private:
   enum class ScreenLifecycle : std::int32_t {
      Closed,
      Open,
      Closing
   };

   struct ScreenState {
      std::string focused;
      ScreenLifecycle lifecycle = ScreenLifecycle::Closed;
   };
   enum class RequestKind : std::int32_t {
      Push,
      Pop,
      Clear,
      Remove
   };

   struct Request {
      RequestKind kind;
      std::string id;
   };

   void ResetRuntime();
   void ApplyRequests();
   void AdvanceTransitions(bool finishAnimations);
   void CloseScreen(const std::string& id);
   void UpdatePolicy();
   void DispatchInput(const UIInputFrame& input, float unscaledDeltaTime);
   UIInputFrame ApplyInputPolicy(UIInputFrame input);
   UIWidgetComponent* Widget(const std::string& id) const;
   UIWidgetComponent* InteractiveWidget(const std::string& id) const;
   std::string InputScreen() const;
   std::vector<std::string> Focusable(const std::string& screenId) const;
   void SetInteraction(const std::string& screenId, const std::string& hovered);
   void EnsureFocus(const std::string& screenId);
   bool IsClosing(const std::string& id) const;

   SceneWorld& world_;
   std::vector<std::string> hierarchyOrder_;
   std::vector<std::string> stack_;
   std::unordered_map<std::string, ScreenState> screens_;
   std::vector<Request> requests_;
   std::vector<UIElementLayout> inputLayout_;
   std::string captured_;
   std::string lastError_;
   bool dispatching_ = false;
   bool inputLatched_ = true;
   bool gameplayPaused_ = false;
   bool gameplayInputBlocked_ = false;
   float heldDirection_ = 0.0f;
   float repeatElapsed_ = 0.0f;
};
}
