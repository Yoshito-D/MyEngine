#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace GameEngine {

/// @brief 列挙型の状態IDとコールバックを使い、一回の遷移解決につき最大一回だけ遷移する状態マシン。
/// @tparam StateId 呼び出し側が定義する状態の列挙型。
/// @details 状態と遷移規則は停止中に設定し、Startで初期状態を明示する。
///          コールバックは例外を送出せず、このインスタンスと参照先の寿命を維持すること。
template <class StateId> class StateMachine final {
   static_assert(std::is_enum_v<StateId>, "StateMachine requires an enum state ID.");

public:
   /// @brief 状態への入場・毎フレーム更新・退場の処理。空のコールバックは呼び出さない。
   struct State {
      std::function<void()> onEnter;       ///< 状態に入ったときに一回呼び出す処理。
      std::function<void(float)> onUpdate; ///< 状態中に経過秒数を渡して呼び出す処理。
      std::function<void()> onExit;        ///< 状態から出るときに一回呼び出す処理。
   };

   /// @brief 状態が未登録・未開始のマシンを構築する。
   StateMachine() = default;

   /// @brief 所有者を参照するコールバックと実行状態の複製を禁止する。
   StateMachine(const StateMachine&) = delete;

   /// @brief 別のマシンのコールバックと実行状態による上書きを禁止する。
   StateMachine& operator=(const StateMachine&) = delete;

   /// @brief コールバックが保持するマシンのアドレスを変えないよう移動を禁止する。
   StateMachine(StateMachine&&) = delete;

   /// @brief コールバックが参照するマシンの実体を差し替える移動代入を禁止する。
   StateMachine& operator=(StateMachine&&) = delete;

   /// @brief 停止中に状態を登録する。登録だけでは入場処理を実行しない。
   /// @param id 登録する状態ID。
   /// @param state 状態に対応するコールバック。
   /// @return 登録成功時はtrue。実行中またはIDが登録済みなら変更せずfalse。
   bool AddState(StateId id, State state) {
      if (executing_ || currentState_) {
         return false;
      }

      return states_.try_emplace(id, std::move(state)).second;
   }

   /// @brief 停止中に指定状態からの遷移許可一覧を設定または置換する。
   /// @param from 登録済みの遷移元。規則のない状態からの遷移は制限しない。
   /// @param toList 登録済みの遷移先一覧。空なら他状態への遷移を禁止する。
   /// @return 設定成功時はtrue。実行中または未登録IDがあれば変更せずfalse。
   bool SetTransitionRule(StateId from, std::vector<StateId> toList) {
      if (executing_ || currentState_ || !states_.contains(from)) {
         return false;
      }

      for (const auto to : toList) {
         if (!states_.contains(to)) {
            return false;
         }
      }

      transitionRules_.insert_or_assign(from, std::move(toList));
      return true;
   }

   /// @brief 停止中に、通常の状態遷移後に呼び出す外部通知を設定する。
   /// @param callback 旧状態と新状態を値で受け取る処理。空なら通知を解除する。
   /// @return 設定成功時はtrue。実行中なら変更せずfalse。
   bool SetTransitionCallback(std::function<void(StateId, StateId)> callback) {
      if (executing_ || currentState_) {
         return false;
      }

      transitionCallback_ = std::move(callback);
      return true;
   }

   /// @brief 指定した初期状態へ入り、更新を開始する。通常の遷移通知は呼び出さない。
   /// @param initialState 登録済みの初期状態ID。
   /// @return 開始成功時はtrue。開始済み、再入、未登録IDの場合は変更せずfalse。
   bool Start(StateId initialState) {
      if (executing_ || currentState_ || !states_.contains(initialState)) {
         return false;
      }

      ExecutionGuard guard(executing_);
      requests_.clear();
      currentState_ = initialState;
      const auto& state = states_.at(initialState);

      if (state.onEnter) {
         state.onEnter();
      }

      return true;
   }

   /// @brief 現状態の退場処理を実行し、登録を維持して未開始へ戻す。
   /// @details 通常の遷移通知は呼び出さず、退場処理中に追加された要求も破棄する。
   /// @return 停止成功時はtrue。未開始またはコールバックからの再入ならfalse。
   bool Stop() {
      if (executing_ || !currentState_) {
         return false;
      }

      ExecutionGuard guard(executing_);
      const auto& state = states_.at(*currentState_);

      if (state.onExit) {
         state.onExit();
      }

      currentState_.reset();
      requests_.clear();
      return true;
   }

   /// @brief 次回の遷移解決に向けて候補を要求する。同じ状態への複数要求は最高優先度を保持する。
   /// @param id 登録済みの候補状態ID。
   /// @param priority 大きい値を優先。同優先度は列挙値が小さい状態を優先する。
   /// @return 要求を保持した場合はtrue。未開始または未登録IDの場合はfalse。
   /// @details 次回のResolveRequestsまたはUpdateで、解決時の現状態に対する遷移許可を判定する。
   ///          自分自身への要求は再入場しない。
   bool RequestState(StateId id, std::int32_t priority = 0) {
      if (!currentState_ || !states_.contains(id)) {
         return false;
      }

      auto [it, inserted] = requests_.try_emplace(id, priority);

      if (!inserted && priority > it->second) {
         it->second = priority;
      }

      return true;
   }

   /// @brief 現在の状態IDを値で取得する。
   /// @return 未開始または停止済みならstd::nullopt、それ以外は現在の状態ID。
   std::optional<StateId> GetCurrentState() const { return currentState_; }

   /// @brief 未処理の遷移要求だけを破棄する。現状態や登録内容は変更しない。
   void ClearRequests() { requests_.clear(); }

   /// @brief 遷移要求を一回解決し、状態の入退場と外部通知だけを実行する。onUpdateは呼び出さない。
   /// @return 状態が変わった場合はtrue。未開始、再入、有効な遷移がない場合はfalse。
   /// @details コールバック中の新規要求は、次回のResolveRequestsまたはUpdateまで解決しない。
   bool ResolveRequests() {
      if (executing_ || !currentState_) {
         return false;
      }

      ExecutionGuard guard(executing_);
      const auto previousState = currentState_;
      Resolve();
      return currentState_ != previousState;
   }

   /// @brief 遷移要求を解決し、確定した状態を一回更新する。未開始と再入は何もしない。
   /// @param deltaTime 経過秒数。負または非有限なら0秒として更新する。
   /// @details 旧Exit→状態更新→新Enter→外部通知→新Updateの順に実行する。
   ///          コールバック内の新規要求は、次回のResolveRequestsまたはUpdateへ持ち越す。
   void Update(float deltaTime) {
      if (executing_ || !currentState_) return;

      ExecutionGuard guard(executing_);
      Resolve();
      const auto& state = states_.at(*currentState_);

      if (state.onUpdate) {
         state.onUpdate(std::isfinite(deltaTime) ? std::max(deltaTime, 0.0f) : 0.0f);
      }
   }

private:
   /// @brief コールバックからの再入を防ぎ、スコープを抜けると実行中フラグを戻す。
   struct ExecutionGuard {
      explicit ExecutionGuard(bool& executing) : executing_(executing) { executing_ = true; }

      ~ExecutionGuard() { executing_ = false; }

      bool& executing_;
   };

   bool CanTransition(StateId to) const {
      if (to == *currentState_) {
         return true;
      }

      const auto it = transitionRules_.find(*currentState_);

      // 現状態に規則がない場合は制限なしとし、必要な状態だけ許可リストを定義できるようにする。
      return it == transitionRules_.end() || std::find(it->second.begin(), it->second.end(), to) != it->second.end();
   }

   void Resolve() {
      std::optional<StateId> bestState;
      std::int32_t bestPriority = 0;

      for (const auto& [id, priority] : requests_) {
         // Exit内の要求も次回の現状態で検証し、古い状態では許可されていた遷移を通さない。
         if (!CanTransition(id)) {
            continue;
         }

         // 同優先度は列挙値順で決め、unordered_mapの反復順に遷移結果を依存させない。
         if (!bestState || priority > bestPriority || (priority == bestPriority && id < *bestState)) {
            bestState = id;
            bestPriority = priority;
         }
      }

      // コールバック中の新規要求を次のResolveへ残せるよう、遷移通知より先に今回分を消費する。
      requests_.clear();

      if (!bestState || bestState == currentState_) return;

      const StateId previousState = *currentState_;
      const auto& previous = states_.at(previousState);

      // 旧Exit→状態更新→新Enter→外部通知の順を固定し、各処理から見える現状態を明確にする。
      if (previous.onExit) {
         previous.onExit();
      }

      currentState_ = *bestState;
      const auto& next = states_.at(*bestState);

      if (next.onEnter) {
         next.onEnter();
      }

      if (transitionCallback_) {
         transitionCallback_(previousState, *currentState_);
      }
   }

   std::unordered_map<StateId, State> states_;                         ///< 状態IDごとの登録済みコールバック。
   std::unordered_map<StateId, std::vector<StateId>> transitionRules_; ///< 状態ごとの遷移先許可一覧。
   std::unordered_map<StateId, std::int32_t> requests_;       ///< 次回の遷移解決に使う状態ごとの最高要求優先度。
   std::optional<StateId> currentState_;                      ///< 未開始を区別できる現在の状態ID。
   std::function<void(StateId, StateId)> transitionCallback_; ///< 通常遷移後の外部通知。
   bool executing_ = false;                                   ///< 開始・停止・更新コールバックの実行中か。
};

} // namespace GameEngine
