#pragma once
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
	  std::function<void()> onEnter;   // 状態切替時に1回
	  std::function<void()> onUpdate;  // 状態中は毎フレーム
   };

   /// @brief 状態を登録
   /// @param name 状態名
   /// @param onEnter 状態に入ったときに呼ばれる関数
   /// @param onUpdate 状態中に毎フレーム呼ばれる関数
   void AddState(const std::string& name,
	  std::function<void()> onEnter = nullptr,
	  std::function<void()> onUpdate = nullptr);

   /// @brief 状態リクエストを追加
   /// @param stateName 状態名
   /// @param priority 優先度（大きいほど優先される）
   void RequestState(const std::string& stateName, int priority);

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
   std::unordered_map<std::string, int> requests_;
   std::unordered_map<std::string, State> states_;
   std::string currentState_ = "Idle";
   std::unordered_map<std::string, std::vector<std::string>> transitionRules_;
};
