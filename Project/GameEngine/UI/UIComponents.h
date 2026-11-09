#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include <array>
#include <functional>
#include <string>
#include <utility>

namespace GameEngine {

/// @brief Canvasの論理寸法。単位はUIピクセル。
struct UICanvasSettings {
   std::array<float, 2> referenceSize{1280.0f, 720.0f};
};

/// @brief 型付き設定を保存JSONへ変換する。
void to_json(nlohmann::json& data, const UICanvasSettings& value);
/// @brief 保存JSONから型付き設定を復元する。
void from_json(const nlohmann::json& data, UICanvasSettings& value);

/// @brief 親矩形基準の配置と画面内の識別キー。
struct UIRectTransformSettings {
   std::string elementKey;
   std::array<float, 2> anchor{0.0f, 0.0f};
   std::array<float, 2> pivot{0.0f, 0.0f};
   std::array<float, 2> position{0.0f, 0.0f}; ///< 親アンカーからの位置（UIピクセル）。
   std::array<float, 2> size{240.0f, 48.0f};  ///< 矩形寸法（UIピクセル）。
   std::int32_t sortingOrder = 0;
};

/// @brief 型付き設定を保存JSONへ変換する。
void to_json(nlohmann::json& data, const UIRectTransformSettings& value);
/// @brief 保存JSONから型付き設定を復元する。
void from_json(const nlohmann::json& data, UIRectTransformSettings& value);

/// @brief 子孫へ継承する表示・操作状態。
struct UIGroupSettings {
   bool visible = true;
   bool interactable = true;
   float opacity = 1.0f;
};

/// @brief 型付き設定を保存JSONへ変換する。
void to_json(nlohmann::json& data, const UIGroupSettings& value);
/// @brief 保存JSONから型付き設定を復元する。
void from_json(const nlohmann::json& data, UIGroupSettings& value);

/// @brief 子の固定サイズを保った縦横整列。余白と間隔はUIピクセル。
struct UILayoutGroupSettings {
   bool horizontal = false;
   std::array<float, 2> padding{16.0f, 16.0f};
   float spacing = 12.0f;
};

/// @brief 型付き設定を保存JSONへ変換する。
void to_json(nlohmann::json& data, const UILayoutGroupSettings& value);
/// @brief 保存JSONから型付き設定を復元する。
void from_json(const nlohmann::json& data, UILayoutGroupSettings& value);

/// @brief 画面の開閉と下画面への影響。
struct UIScreenSettings {
   std::string defaultFocusKey;
   bool horizontalNavigation = false;
   bool drawBelow = true;
   bool blocksBelowInput = true;
   bool pausesGameplay = false;
   bool openOnStart = false;
};

/// @brief 型付き設定を保存JSONへ変換する。
void to_json(nlohmann::json& data, const UIScreenSettings& value);
/// @brief 保存JSONから型付き設定を復元する。
void from_json(const nlohmann::json& data, UIScreenSettings& value);

/// @brief 操作部品の共通設定。valueは初期値であり実行中の値は別に保持する。
struct UIWidgetSettings {
   bool interactable = true;
   std::int32_t navigationOrder = 0;
   float minimum = 0.0f;
   float maximum = 1.0f;
   float step = 0.05f;
   float value = 0.0f;
   std::array<float, 4> normalColor{0.18f, 0.23f, 0.32f, 1.0f};
   std::array<float, 4> selectedColor{0.15f, 0.48f, 0.78f, 1.0f};
   std::array<float, 4> pressedColor{0.1f, 0.32f, 0.55f, 1.0f};
   std::array<float, 4> disabledColor{0.18f, 0.18f, 0.18f, 0.5f};
};

/// @brief 型付き設定を保存JSONへ変換する。
void to_json(nlohmann::json& data, const UIWidgetSettings& value);
/// @brief 保存JSONから型付き設定を復元する。
void from_json(const nlohmann::json& data, UIWidgetSettings& value);

/// @brief 設定値を有限の範囲へ正規化する。単位は各Settingsの定義に従う。
void NormalizeUISettings(nlohmann::json& data);

#ifdef USE_IMGUI
/// @brief 共通の型付き設定をInspectorから編集する。変更した場合true。
bool DrawUISettingsInspector(const char* typeName, nlohmann::json& data);
#endif

/// @brief 型付き設定の保存・復元とUI更新区分を共有する基底。
template <typename Settings> class UIConfiguredComponent : public IObjectComponent {
public:
   /// @brief UI実時間で更新する区分を返す。
   ObjectUpdateDomain GetUpdateDomain() const override { return ObjectUpdateDomain::UI; }

   /// @brief 現在の編集設定を読み取り専用で取得する。
   const Settings& DescribeSettings() const { return settings_; }

   /// @brief 検証した設定を適用する。実行時の入力状態は変更しない。
   void Configure(const Settings& settings) {
      auto data = nlohmann::json(settings);
      NormalizeUISettings(data);
      settings_ = data.template get<Settings>();
   }

   /// @brief 実行履歴を含まない設定JSONを返す。
   nlohmann::json Serialize() const override { return settings_; }

   /// @brief 保存JSONを検証して設定を復元する。
   void Deserialize(const nlohmann::json& data) override { Configure(data.template get<Settings>()); }

#ifdef USE_IMGUI
   /// @brief 部品の設定を既存Inspectorへ表示する。
   void DrawInspector() override {
      auto data = Serialize();
      if (DrawUISettingsInspector(GetTypeName(), data)) {
         Deserialize(data);
      }
   }
#endif

private:
   Settings settings_{};
};

/// @brief 論理画面を定義するUIルート。
class UICanvasComponent final : public UIConfiguredComponent<UICanvasSettings> {
public:
   static constexpr const char* kTypeName = "UICanvasComponent";
   static constexpr ComponentDisplayName kDisplayName{"UIキャンバス", "UI Canvas"};

   /// @brief 保存・検索用の型名を返す。
   const char* GetTypeName() const override { return kTypeName; }
};

/// @brief 配置・描画・ヒット判定が共有する矩形定義。
class UIRectTransformComponent final : public UIConfiguredComponent<UIRectTransformSettings> {
public:
   static constexpr const char* kTypeName = "UIRectTransformComponent";
   static constexpr ComponentDisplayName kDisplayName{"UI矩形", "UI Rect Transform"};

   /// @brief 保存・検索用の型名を返す。
   const char* GetTypeName() const override { return kTypeName; }
};

/// @brief 子孫の表示・操作・不透明度をまとめて制御する。
class UIGroupComponent final : public UIConfiguredComponent<UIGroupSettings> {
public:
   static constexpr const char* kTypeName = "UIGroupComponent";
   static constexpr ComponentDisplayName kDisplayName{"UIグループ", "UI Group"};

   /// @brief 保存・検索用の型名を返す。
   const char* GetTypeName() const override { return kTypeName; }
};

/// @brief 直下の矩形を縦または横へ整列する。
class UILayoutGroupComponent final : public UIConfiguredComponent<UILayoutGroupSettings> {
public:
   static constexpr const char* kTypeName = "UILayoutGroupComponent";
   static constexpr ComponentDisplayName kDisplayName{"UI自動整列", "UI Layout Group"};

   /// @brief 保存・検索用の型名を返す。
   const char* GetTypeName() const override { return kTypeName; }
};

/// @brief スタックで開閉する画面。実体の所有権はSceneObjectStoreに残す。
class UIScreenComponent final : public UIConfiguredComponent<UIScreenSettings> {
public:
   static constexpr const char* kTypeName = "UIScreenComponent";
   static constexpr ComponentDisplayName kDisplayName{"UI画面", "UI Screen"};

   /// @brief 保存・検索用の型名を返す。
   const char* GetTypeName() const override { return kTypeName; }
};

/// @brief 値変更へ割り当てる方向軸。Noneの場合は画面内の選択移動へ渡す。
enum class UIAdjustmentAxis : std::int32_t {
   None,
   Horizontal,
   Vertical
};

/// @brief 描画側へ渡す操作部品の表示情報。保存値・共有マテリアルは変更しない。
struct UIWidgetVisual {
   std::array<float, 4> color{1.0f, 1.0f, 1.0f, 1.0f};
   float fillAmount = 0.0f; ///< 矩形の横幅に対する0～1の表示割合。
   std::array<float, 4> fillColor{0.2f, 0.7f, 0.95f, 0.75f};
};

/// @brief 入力配送が扱う操作状態。コールバックとともに保存対象外。
class UIWidgetComponent : public UIConfiguredComponent<UIWidgetSettings> {
public:
   /// @brief 一つのObjectへ複数の操作部品を接続することを拒否する。
   void OnAttach() final;

   /// @brief 決定入力を実行する。派生部品が一度だけ通知する。
   virtual void Activate() = 0;

   /// @brief 左右入力による値の変更。ボタンでは何もしない。
   virtual bool Adjust([[maybe_unused]] float direction) { return false; }

   /// @brief 値変更に使用する方向軸を返す。
   virtual UIAdjustmentAxis GetAdjustmentAxis() const { return UIAdjustmentAxis::None; }

   /// @brief ドラッグ位置を0～1で適用する。スライダー以外では何もしない。
   virtual void SetNormalizedValue([[maybe_unused]] float value) {}

   /// @brief 画面破棄前に外部へのコールバックを解除する。
   virtual void ClearCallbacks() = 0;
   /// @brief 現在のフォーカス・ホバー・押下状態を設定する。
   void SetInteraction(bool focused, bool hovered, bool pressed);
   /// @brief 現在の見た目へ適用する色を返す。元のマテリアルは変更しない。
   std::array<float, 4> GetVisualColor() const;
   /// @brief 入力遮断状態も考慮した描画情報を返す。blockedは親や画面による操作遮断。
   virtual UIWidgetVisual GetVisual(bool blocked = false) const;
   /// @brief デタッチ時にコールバックと入力状態を解除する。
   void OnDetach() override;

private:
   bool focused_ = false;
   bool hovered_ = false;
   bool pressed_ = false;
};

/// @brief クリックとキー・パッド決定を同じ通知にまとめるボタン。
class UIButtonComponent final : public UIWidgetComponent {
public:
   static constexpr const char* kTypeName = "UIButtonComponent";
   static constexpr ComponentDisplayName kDisplayName{"UIボタン", "UI Button"};

   /// @brief 保存・検索用の型名を返す。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 有効な決定時に呼ぶ処理を登録する。空関数で解除する。
   void SetOnClick(std::function<void()> callback) { onClick_ = std::move(callback); }

   /// @brief 登録された決定処理を一度呼ぶ。
   void Activate() override;

   /// @brief 外部への決定処理を解除する。
   void ClearCallbacks() override { onClick_ = {}; }

private:
   std::function<void()> onClick_;
};

/// @brief キー・パッドとドラッグで数値を変更するスライダー。
class UISliderComponent final : public UIWidgetComponent {
public:
   static constexpr const char* kTypeName = "UISliderComponent";
   static constexpr ComponentDisplayName kDisplayName{"UIスライダー", "UI Slider"};

   /// @brief 保存・検索用の型名を返す。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 保存された初期値から実行値を初期化する。
   void OnSceneLoaded(SceneWorld&) override;

   /// @brief 現在の実行値を返す。
   float GetValue() const { return value_; }

   /// @brief 値を範囲へ制限して設定する。変化時だけ必要に応じて通知する。
   void SetValue(float value, bool notify = true);

   /// @brief 値変更処理を登録する。画面内の実行値を受け取る。
   void SetOnValueChanged(std::function<void(float)> callback) { onChanged_ = std::move(callback); }

   /// @brief スライダーの決定入力は値を変えない。
   void Activate() override {}

   /// @brief 指定方向へ設定済みステップ分だけ変更する。
   bool Adjust(float direction) override;

   /// @brief 左右方向を値変更へ使用する。
   UIAdjustmentAxis GetAdjustmentAxis() const override { return UIAdjustmentAxis::Horizontal; }

   /// @brief 現在値の割合を含む表示情報を返す。
   UIWidgetVisual GetVisual(bool blocked = false) const override;
   /// @brief ドラッグ位置を範囲内の値へ変換する。
   void SetNormalizedValue(float value) override;

   /// @brief 外部への値変更処理を解除する。
   void ClearCallbacks() override { onChanged_ = {}; }

private:
   float value_ = 0.0f;
   std::function<void(float)> onChanged_;
};

/// @brief 決定操作で真偽値を切り替えるトグル。
class UIToggleComponent final : public UIWidgetComponent {
public:
   static constexpr const char* kTypeName = "UIToggleComponent";
   static constexpr ComponentDisplayName kDisplayName{"UIトグル", "UI Toggle"};

   /// @brief 保存・検索用の型名を返す。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 保存された初期値から実行値を初期化する。
   void OnSceneLoaded(SceneWorld&) override;

   /// @brief 現在の実行値を返す。
   bool GetValue() const { return value_; }

   /// @brief 真偽値を設定し、変化時だけ必要に応じて通知する。
   void SetValue(bool value, bool notify = true);

   /// @brief 値変更処理を登録する。
   void SetOnValueChanged(std::function<void(bool)> callback) { onChanged_ = std::move(callback); }

   /// @brief 現在値を反転する。
   void Activate() override;

   /// @brief 外部への値変更処理を解除する。
   void ClearCallbacks() override { onChanged_ = {}; }

   /// @brief 真偽値の表示割合を含む表示情報を返す。
   UIWidgetVisual GetVisual(bool blocked = false) const override;

private:
   bool value_ = false;
   std::function<void(bool)> onChanged_;
};

/// @brief Objectに接続された操作部品を返す。未接続ならnullptr。
UIWidgetComponent* GetUIWidget(Object& object);

/// @brief Objectに接続された操作部品を読み取り専用で返す。派生型も検索する。
const UIWidgetComponent* GetUIWidget(const Object& object);

} // namespace GameEngine
