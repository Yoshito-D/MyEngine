#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <algorithm>

/// @brief 状態マシンクラス
class StateMachine {
public:
   /// @brief コンストラクタ
   StateMachine();

   /// @brief 状態構造体
   struct State {
      std::function<void()> onEnter;  // 状態切替時に1回
      std::function<void()> onUpdate; // 状態中は毎フレーム
      std::function<void()> onExit;   // 状態切替時に1回
   };

   /// @brief 状態を登録
   /// @param name 状態名
   /// @param onEnter 状態に入ったときに呼ばれる関数
   /// @param onUpdate 状態中に毎フレーム呼ばれる関数
   /// @param onExit 状態から出るときに呼ばれる関数
   void AddState(const std::string& name, std::function<void()> onEnter = nullptr,
      std::function<void()> onUpdate = nullptr, std::function<void()> onExit = nullptr);

   /// @brief 状態変更時の通知コールバックを設定
   /// @param callback 旧状態名と新状態名を受け取る関数
   void SetTransitionCallback(std::function<void(const std::string&, const std::string&)> callback);

   /// @brief 状態リクエストを追加
   /// @param stateName 状態名
   /// @param priority 優先度（大きいほど優先される）
   void RequestState(const std::string& stateName, std::int32_t priority);

   /// @brief 現在の状態名を取得
   const std::string& GetCurrentState() const { return currentState_; }

   /// @brief 遷移ルールを追加
   /// @param from 遷移元の状態名
   /// @param toList 遷移先の状態名リスト
   void AddTransitionRule(const std::string& from, const std::vector<std::string>& toList);

   /// @brief Update を呼ぶだけで Resolve + onUpdate を実行
   void Update();

   /// @brief リクエストをクリア
   void Clear();

private:
   const std::string& Resolve();
   bool CanTransition(const std::string& newState) const;

private:
   std::unordered_map<std::string, std::int32_t> requests_; ///< 次の状態解決で比較する状態名ごとの最高要求優先度。
   std::unordered_map<std::string, State> states_;          ///< プレイヤースロットごとの評価済みアクション状態。
   std::string currentState_ = "Idle";                      ///< 現在の状態名。
   /// @brief 現在状態から遷移可能な状態名の許可一覧。
   std::unordered_map<std::string, std::vector<std::string>> transitionRules_;
   /// @brief 状態切り替え後に旧状態名と新状態名を通知する処理。
   std::function<void(const std::string&, const std::string&)> transitionCallback_;
   bool hasEnteredCurrentState_ = false; ///< 現在状態の開始処理を呼び出し済みかどうか。
};
