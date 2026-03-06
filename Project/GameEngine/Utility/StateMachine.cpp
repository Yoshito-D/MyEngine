#include "StateMachine.h"

StateMachine::StateMachine() {}

// --------------------------------------------------------
// 状態登録
// --------------------------------------------------------
void StateMachine::AddState(const std::string& name,
   std::function<void()> onEnter,
   std::function<void()> onUpdate)
{
   states_[name] = { onEnter, onUpdate };
}

// --------------------------------------------------------
// 状態リクエスト追加
// --------------------------------------------------------
void StateMachine::RequestState(const std::string& stateName, int priority)
{
   if (!CanTransition(stateName)) return;

   auto it = requests_.find(stateName);
   if (it != requests_.end()) {
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

   int bestPriority = -999999;
   std::string bestState = currentState_;

   for (const auto& req : requests_) {
	  if (req.second > bestPriority) {
		 bestPriority = req.second;
		 bestState = req.first;
	  }
   }

   // 状態が切り替わった場合に onEnter を呼ぶ
   if (bestState != currentState_) {
	  currentState_ = bestState;
	  auto it = states_.find(bestState);
	  if (it != states_.end() && it->second.onEnter) {
		 it->second.onEnter();
	  }
   }

   requests_.clear();
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
