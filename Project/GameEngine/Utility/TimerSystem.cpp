#include "TimerSystem.h"

namespace GameEngine {

TimerSystem::TimerId TimerSystem::SetTimeout(float delaySec, std::function<void()> callback) {
   Timer t{};
   t.id = NextId();
   t.remaining = (delaySec < 0.0f) ? 0.0f : delaySec;
   t.interval = 0.0f; // 一度きり
   t.repeat = 1;
   t.callback = std::move(callback);
   Timers().emplace(t.id, std::move(t));
   return t.id;
}

TimerSystem::TimerId TimerSystem::SetInterval(float intervalSec, std::function<void()> callback, int repeat) {
   Timer t{};
   t.id = NextId();
   float clamped = (intervalSec < 0.0f) ? 0.0f : intervalSec;
   t.remaining = clamped;
   t.interval = clamped;
   t.repeat = repeat; // -1: 無限
   t.callback = std::move(callback);
   Timers().emplace(t.id, std::move(t));
   return t.id;
}

void TimerSystem::Clear(TimerId id) {
   Timers().erase(id);
}

void TimerSystem::ClearAll() {
   Timers().clear();
}

void TimerSystem::Pause(TimerId id) {
   auto it = Timers().find(id);
   if (it != Timers().end()) it->second.paused = true;
}

void TimerSystem::Resume(TimerId id) {
   auto it = Timers().find(id);
   if (it != Timers().end()) it->second.paused = false;
}

bool TimerSystem::Exists(TimerId id) {
   return Timers().contains(id);
}

void TimerSystem::Update(float deltaTime) {
   if (Timers().empty()) return;

   std::vector<TimerId> toErase;

   for (auto& [id, t] : Timers()) {
	  if (t.paused) continue;

	  float dt = (deltaTime < 0.0f) ? 0.0f : deltaTime;
	  t.remaining -= dt;
	  if (t.remaining > 0.0f) continue;

	  // 発火
	  if (t.callback) t.callback();

	  // 回数消費
	  if (t.repeat > 0) {
		 t.repeat--;
	  }

	  // 継続 or 削除
	  if (t.interval > 0.0f && (t.repeat != 0)) {
		 // 周期継続（オーバー分も次へ繰越）
		 t.remaining += t.interval;
	  } else {
		 // 完了
		 toErase.push_back(id);
	  }
   }

   for (auto id : toErase) {
	  Timers().erase(id);
   }
}

void TimerSystem::Tick() {
   Update(EngineContext::GetDeltaTime());
}

TimerSystem::TimerId TimerSystem::NextId() {
   static TimerId s_id = 1;
   return s_id++;
}

std::unordered_map<TimerSystem::TimerId, TimerSystem::Timer>& TimerSystem::Timers() {
   static std::unordered_map<TimerId, Timer> s_timers;
   return s_timers;
}

} // namespace GameEngine
