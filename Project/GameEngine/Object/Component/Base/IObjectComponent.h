#pragma once

#include "GameEngine/Object/Component/Base/ParameterComponent.h"

#include <nlohmann/json.hpp>
#include <string>

namespace GameEngine {
class Object;
class Camera;
class SceneWorld;
class ComponentContainer;

/// @brief 描画キュー作成前にシーンのコンポーネントへ渡す情報。
struct SceneDrawContext {
   Camera* camera; ///< 現在フレームの描画カメラ。未設定ならnullptr。
   bool editing;   ///< シミュレーションを実行していない編集中の描画か。
};

/// @brief コンポーネント型のエディター表示名
struct ComponentDisplayName {
   const char* japanese = ""; ///< 日本語を選択しているときに表示する文字列。
   const char* english = "";  ///< 英語を選択しているときに表示する文字列。
};

#ifdef USE_IMGUI
const char* LocalizeEditorText(const char* japanese, const char* english);
std::string LocalizeObjectComponentTypeName(const char* typeName);
std::string MakeObjectComponentHeaderLabel(const char* typeName);
#endif

/// @brief シーン全体で共有する更新順。各フェーズは全オブジェクトで完了してから次へ進む。
enum class ObjectUpdatePhase {
   Preparation,
   Input,
   TargetSelection,
   Forces,
   Physics,
   Contacts,
   Control,
   Default,
   Triggers,
   TriggerEvents,
   Progress,
   Presentation,
   Effects,
   Count,
};

/// @brief ゲーム進行と実時間UIを分ける更新区分。
enum class ObjectUpdateDomain {
   Gameplay,
   UI
};

/// @brief Objectへアタッチできるコンポーネントの共通インターフェース
class IObjectComponent : public ParameterComponent {
public:
   /// @brief パラメータの保存先を区別するコンポーネント領域を返す。
   const char* GetParameterDomain() const override { return "Object"; }

   /// @brief 共有パラメータの識別に使うコンポーネント型名を返す。
   const char* GetParameterType() const override { return GetTypeName(); }

   /// @brief 未接続・未確保の状態を構築する。
   IObjectComponent() = default;

   /// @brief 所有者への接続やGPU実行状態を別の実体へ複製することを禁止する。
   IObjectComponent(const IObjectComponent&) = delete;

   /// @brief 所有境界を迂回するコピー代入を禁止する。
   IObjectComponent& operator=(const IObjectComponent&) = delete;

   /// @brief 派生コンポーネントを基底ポインター経由で安全に破棄する
   virtual ~IObjectComponent() = default;

   /// @brief コンポーネントの型名を取得する（純粋仮想）
   virtual const char* GetTypeName() const = 0;

   /// @brief 接続先のオブジェクトを取得する。
   /// @pre HasOwner()がtrueであること。
   Object& GetOwner() { return *owner_; }

   /// @brief 接続先のオブジェクトを読み取り専用で取得する。
   /// @pre HasOwner()がtrueであること。
   const Object& GetOwner() const { return *owner_; }

   /// @brief オーナーが設定されているか確認する
   bool HasOwner() const { return owner_ != nullptr; }

   /// @brief コンポーネントが更新対象として有効か取得する
   bool IsEnabled() const { return isEnabled_; }

   /// @brief 有効状態を変更し、必要なライフサイクル通知を行う
   void SetEnabled(bool enabled);

   /// @brief オブジェクトへアタッチされた直後に呼ばれる
   virtual void OnAttach() {}

   /// @brief オブジェクトからデタッチされる直前に呼ばれる
   virtual void OnDetach() {}

   /// @brief コンポーネントが有効化された直後に呼ばれる
   virtual void OnEnable() {}

   /// @brief コンポーネントが無効化された直後に呼ばれる
   virtual void OnDisable() {}

   /// @brief シーン内の全オブジェクト生成後に参照を解決する
   /// @param sceneWorld このコンポーネントを所有するシーンワールド
   virtual void OnSceneLoaded(SceneWorld& sceneWorld) { OnReferencesChanged(sceneWorld); }

   /// @brief 編集後に非所有参照だけを再解決する（ゲーム進行や保存設定は変更しない）
   /// @param sceneWorld 現在のシーンワールド
   virtual void OnReferencesChanged(SceneWorld& sceneWorld) { (void)sceneWorld; }

   /// @brief 更新時間の区分を返す。既定はゲーム進行。
   virtual ObjectUpdateDomain GetUpdateDomain() const { return ObjectUpdateDomain::Gameplay; }

   /// @brief 操作や物理の前に、このフレームのゲーム進行に必要な状態を準備する。
   virtual void PrepareGameplayFrame() {}

   /// @brief 更新フェーズを返す。同じフェーズ内では追加順を維持する。
   virtual ObjectUpdatePhase GetUpdatePhase() const { return ObjectUpdatePhase::Default; }

   /// @brief 指定された時間だけ部品の実行状態を更新する。
   /// @param deltaTime 更新時間（秒）。
   virtual void Update([[maybe_unused]] float deltaTime) {}

   /// @brief 描画キューの構築前に、表示状態や提出データを準備する。
   /// @param context 現在フレームのカメラと編集状態。ここでは部品の追加・削除を行わない。
   virtual void PrepareDraw([[maybe_unused]] const SceneDrawContext& context) {}

   /// @brief コンポーネント設定をJSONへ変換する
   virtual nlohmann::json Serialize() const = 0;

   /// @brief JSONからコンポーネント設定を復元する
   virtual void Deserialize(const nlohmann::json& data) = 0;

#ifdef USE_IMGUI

   /// @brief インスペクター描画（オーナーはGetOwner()で取得）
   virtual void DrawInspector() {}
#endif

private:
   // 所有コンテナとowner_の対応を保つため、付け外しはコンテナだけに許可する。
   friend class ComponentContainer;

   /// @brief コンポーネントをオブジェクトにアタッチする
   void Attach(Object& owner);

   /// @brief コンポーネントをオブジェクトからデタッチする
   void Detach();

   Object* owner_ = nullptr; ///< この部品の接続先となる所有オブジェクトへの参照。
   bool isEnabled_ = true;   ///< コンポーネントが更新対象として有効かどうか。
};
}
