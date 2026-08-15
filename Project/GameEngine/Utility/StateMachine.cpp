#include "StateMachine.h"

#include <limits>

StateMachine::StateMachine() {}

// --------------------------------------------------------
// 状態登録
// --------------------------------------------------------
void StateMachine::AddState(const std::string& name,
   std::function<void()> onEnter,
   std::function<void()> onUpdate,
   std::function<void()> onExit
   )
{
   states_[name] = { onEnter, onUpdate, onExit };

   if (name == currentState_ && !hasEnteredCurrentState_) {
	  // 初期状態名が登録より先に設定されていた場合も、登録時にEnterを一度だけ補完する。
	  hasEnteredCurrentState_ = true;
	  if (states_[name].onEnter) {
		 states_[name].onEnter();
	  }
   }
}

void StateMachine::SetTransitionCallback(
   std::function<void(const std::string&, const std::string&)> callback)
{
   transitionCallback_ = std::move(callback);
}

// --------------------------------------------------------
// 状態リクエスト追加
// --------------------------------------------------------
void StateMachine::RequestState(const std::string& stateName, int priority)
{
	  if (states_.find(stateName) == states_.end()) return;
   if (!CanTransition(stateName)) return;

   auto it = requests_.find(stateName);
   if (it != requests_.end()) {
	  // 同じ状態への複数要求は最高優先度だけを残し、要求回数で結果を変えない。
	  if (priority > it->second) it->second = priority;
   } else {
	  requests_.insert({ stateName, priority });
   }
}

// --------------------------------------------------------
// 最優先状態を決定
// --------------------------------------------------------
const std::string& StateMachine::Resolve()
{
   if (requests_.empty()) return currentState_;

	  int bestPriority = std::numeric_limits<int>::lowest();
   std::string bestState = currentState_;
   bool hasBestState = false;

   for (const auto& req : requests_) {
	  // 同優先度は状態名順で決め、unordered_mapの反復順に遷移結果を依存させない。
	  if (!hasBestState || req.second > bestPriority ||
		 (req.second == bestPriority && req.first < bestState)) {
		 bestPriority = req.second;
		 bestState = req.first;
		 hasBestState = true;
	  }
   }

   // コールバック中の新規要求を次のResolveへ残せるよう、遷移通知より先に今回分を消費する。
   requests_.clear();

   // 状態が切り替わった場合に onEnter を呼ぶ
   if (bestState != currentState_) {
	  const std::string previousState = currentState_;
	  // 旧Exit→状態更新→新Enter→外部通知の順を固定し、各処理から見えるcurrentStateを明確にする。
		auto currentIt = states_.find(currentState_);
	  if (currentIt != states_.end() && currentIt->second.onExit) {
		 currentIt->second.onExit();
	  }

	  currentState_ = bestState;
	  auto it = states_.find(bestState);
	  if (it != states_.end() && it->second.onEnter) {
		 it->second.onEnter();
	  }
	  hasEnteredCurrentState_ = true;

	  if (transitionCallback_) {
		 transitionCallback_(previousState, currentState_);
	  }
   }

   return currentState_;
}

// --------------------------------------------------------
// Update 呼び出しで Resolve + onUpdate
// --------------------------------------------------------
void StateMachine::Update()
{
   const std::string& state = Resolve();
   auto it = states_.find(state);
   if (it != states_.end() && it->second.onUpdate) {
	  it->second.onUpdate();
   }
}

// --------------------------------------------------------
// 遷移ルール追加
// --------------------------------------------------------
void StateMachine::AddTransitionRule(const std::string& from,
   const std::vector<std::string>& toList)
{
   transitionRules_[from] = toList;
}

// --------------------------------------------------------
// 遷移判定
// --------------------------------------------------------
bool StateMachine::CanTransition(const std::string& newState) const
{
   auto it = transitionRules_.find(currentState_);
   // 現状態に規則がない場合は制限なしとし、必要な状態だけ許可リストを定義できるようにする。
   if (it == transitionRules_.end()) return true;

   const auto& allowedList = it->second;
   return std::find(allowedList.begin(), allowedList.end(), newState) != allowedList.end();
}

// --------------------------------------------------------
// リクエストクリア
// --------------------------------------------------------
void StateMachine::Clear()
{
   requests_.clear();
}
