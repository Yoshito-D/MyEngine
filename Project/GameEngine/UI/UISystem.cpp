#include "GameEngine/pch.h"
#include "GameEngine/UI/UISystem.h"
#include "GameEngine/UI/UIComponents.h"
#include "GameEngine/UI/UIAnimationComponent.h"
#include "GameEngine/UI/UIScreenAssetSerializer.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Object/Component/Rendering/RenderComponent.h"
#include "GameEngine/Scene/SceneWorld.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace {
constexpr float kRepeatDelaySeconds = 0.35f;
constexpr float kRepeatIntervalSeconds = 0.1f;
}

namespace GameEngine {
namespace {
std::vector<UIAnimationComponent*> ScreenAnimations(SceneWorld& world, const std::string& rootId) {
   std::vector<UIAnimationComponent*> result;

   for (auto* object : world.GetObjectStore().GetObjects()) {
      auto* animation = object->GetComponent<UIAnimationComponent>();

      if (!animation) {
         continue;
      }

      const Object* current = object;
      std::unordered_set<std::string> visited;

      while (current && visited.insert(current->GetEntityId()).second) {
         if (current->GetEntityId() == rootId) {
            result.push_back(animation);
            break;
         }

         if (current->GetComponent<UICanvasComponent>()) {
            break;
         }

         current = world.FindObjectById(current->GetParentEntityId());
      }
   }

   return result;
}

void PlayClickAnimation(Object* object) {
   auto* animation = object ? object->GetComponent<UIAnimationComponent>() : nullptr;

   if (animation && animation->IsEnabled()) {
      animation->Play(UIAnimationEvent::Click);
   }
}
}

UISystem::UISystem(SceneWorld& world) : world_(world) {
}

UISystem::~UISystem() = default;

UIWidgetComponent* UISystem::Widget(const std::string& id) const {
   auto* object = world_.FindObjectById(id);
   return object ? GetUIWidget(*object) : nullptr;
}

UIWidgetComponent* UISystem::InteractiveWidget(const std::string& id) const {
   auto* widget = Widget(id);

   if (!widget || !widget->IsEnabled() || !widget->DescribeSettings().interactable) {
      return nullptr;
   }

   const auto found = std::find_if(
      inputLayout_.begin(), inputLayout_.end(), [&](const auto& element) { return element.objectId == id; });

   if (found == inputLayout_.end() || !found->visible || !found->interactable || IsClosing(found->screenId)) {
      return nullptr;
   }

   const Object* current = world_.FindObjectById(id);
   std::unordered_set<std::string> visited;

   while (current && visited.insert(current->GetEntityId()).second) {
      const auto* group = current->GetComponent<UIGroupComponent>();

      if (group && group->IsEnabled() && !group->DescribeSettings().interactable) {
         return nullptr;
      }

      current = world_.FindObjectById(current->GetParentEntityId());
   }

   return IsUpdateVisible(widget->GetOwner()) ? widget : nullptr;
}

bool UISystem::IsManaged(const Object& object) const {
   const Object* current = &object;
   std::unordered_set<std::string> visited;

   while (current && visited.insert(current->GetEntityId()).second) {
      if (current->GetComponent<UICanvasComponent>() || current->GetComponent<UIScreenComponent>()) {
         return true;
      }

      current = world_.FindObjectById(current->GetParentEntityId());
   }

   return false;
}

bool UISystem::IsUpdateVisible(const Object& object) const {
   const Object* current = &object;
   bool visible = true;
   std::unordered_set<std::string> visited;

   while (current && visited.insert(current->GetEntityId()).second) {
      if (const auto* group = current->GetComponent<UIGroupComponent>(); group && group->IsEnabled()) {
         visible = visible && group->DescribeSettings().visible;
      }

      if (const auto* render = current->GetComponent<RenderComponent>()) {
         visible = visible && render->IsEnabled() && render->DescribeSettings().visible;
      }

      if (const auto* canvas = current->GetComponent<UICanvasComponent>()) {
         const auto root = std::find(stack_.begin(), stack_.end(), current->GetEntityId());

         if (!visible || !canvas->IsEnabled() || root == stack_.end()) {
            return false;
         }

         for (auto above = std::next(root); above != stack_.end(); ++above) {
            const auto* other = world_.FindObjectById(*above);
            const auto* screen = other ? other->GetComponent<UIScreenComponent>() : nullptr;

            if (screen && screen->IsEnabled() && !screen->DescribeSettings().drawBelow) {
               return false;
            }
         }

         return true;
      }

      current = world_.FindObjectById(current->GetParentEntityId());
   }

   return !IsManaged(object);
}

void UISystem::ResetRuntime() {
   for (auto* object : world_.GetObjectStore().GetObjects()) {
      if (auto* widget = GetUIWidget(*object)) {
         widget->ClearCallbacks();
         widget->SetInteraction(false, false, false);
      }

      if (auto* animation = object->GetComponent<UIAnimationComponent>()) {
         animation->Reset();
      }
   }

   stack_.clear();
   screens_.clear();
   inputLayout_.clear();
   captured_.clear();
   inputLatched_ = true;
   heldDirection_ = repeatElapsed_ = 0.0f;
}

void UISystem::ResetForScene() {
   requests_.clear();
   ResetRuntime();
   hierarchyOrder_.clear();
   gameplayPaused_ = gameplayInputBlocked_ = false;
}

void UISystem::Clear() {
   requests_.push_back({RequestKind::Clear, {}});
}

void UISystem::RemoveScreen(const std::string& rootId) {
   if (const auto* root = world_.FindObjectById(rootId); root && IsManaged(*root)) {
      requests_.push_back({RequestKind::Remove, rootId});
   }
}

void UISystem::InitializeScreens() {
   for (auto* object : world_.GetObjectStore().GetObjects()) {
      const auto* screen = object->GetComponent<UIScreenComponent>();

      if (screen && screen->IsEnabled() && screen->DescribeSettings().openOnStart) {
         Push(object->GetEntityId());
      }
   }
}

bool UISystem::IsClosing(const std::string& id) const {
   const auto state = screens_.find(id);
   return state != screens_.end() && state->second.lifecycle == ScreenLifecycle::Closing;
}

bool UISystem::Push(const std::string& rootId) {
   const auto* root = world_.FindObjectById(rootId);
   const auto* canvas = root ? root->GetComponent<UICanvasComponent>() : nullptr;
   const auto* screen = root ? root->GetComponent<UIScreenComponent>() : nullptr;

   if (!canvas || !screen || !canvas->IsEnabled() || !screen->IsEnabled()) {
      return false;
   }

   if (std::any_of(requests_.begin(), requests_.end(),
          [&](const auto& request) { return request.kind == RequestKind::Push && request.id == rootId; })) {
      return false;
   }

   if (requests_.empty() && std::find(stack_.begin(), stack_.end(), rootId) != stack_.end() && !IsClosing(rootId)) {
      return false;
   }

   requests_.push_back({RequestKind::Push, rootId});
   return true;
}

void UISystem::Pop() {
   requests_.push_back({RequestKind::Pop, {}});
}

void UISystem::CloseScreen(const std::string& id) {
   for (auto* animation : ScreenAnimations(world_, id)) {
      animation->Reset();
   }

   std::erase(stack_, id);

   if (auto state = screens_.find(id); state != screens_.end()) {
      state->second.lifecycle = ScreenLifecycle::Closed;
   }

   captured_.clear();
   inputLatched_ = true;
   heldDirection_ = repeatElapsed_ = 0.0f;
}

void UISystem::ApplyRequests() {
   if (dispatching_ || requests_.empty()) {
      return;
   }

   const auto requests = std::exchange(requests_, {});

   for (const auto& request : requests) {
      switch (request.kind) {
         case RequestKind::Push: {
            const auto* root = world_.FindObjectById(request.id);
            const auto* canvas = root ? root->GetComponent<UICanvasComponent>() : nullptr;
            const auto* screen = root ? root->GetComponent<UIScreenComponent>() : nullptr;

            if (!canvas || !screen || !canvas->IsEnabled() || !screen->IsEnabled()) {
               break;
            }

            auto& state = screens_[request.id];

            if (state.lifecycle == ScreenLifecycle::Open) {
               break;
            }

            if (state.lifecycle == ScreenLifecycle::Closed) {
               stack_.push_back(request.id);
            }

            state.lifecycle = ScreenLifecycle::Open;

            for (auto* animation : ScreenAnimations(world_, request.id)) {
               if (animation->IsEnabled()) {
                  animation->NotifyShown();
               }
            }

            break;
         }
         case RequestKind::Pop:

            if (!stack_.empty() && !IsClosing(stack_.back())) {
               screens_[stack_.back()].lifecycle = ScreenLifecycle::Closing;

               for (auto* animation : ScreenAnimations(world_, stack_.back())) {
                  if (animation->IsEnabled()) {
                     animation->NotifyHiding();
                  }
               }
            }

            break;
         case RequestKind::Clear:
            ResetRuntime();
            break;
         case RequestKind::Remove:

            for (const auto& id : world_.GetObjectStore().GetSubtreeIds(request.id)) {
               if (auto* widget = Widget(id)) {
                  widget->ClearCallbacks();
                  widget->SetInteraction(false, false, false);
               }
            }

            world_.GetObjectStore().DeleteSubtree(request.id);
            break;
      }
   }

   captured_.clear();
   inputLatched_ = true;
   heldDirection_ = repeatElapsed_ = 0.0f;
}

void UISystem::AdvanceTransitions(bool finishAnimations) {
   // 完了判定はフレーム開始・入力配送後だけで行い、描画回数や編集用の矩形計算へ依存させない。
   const auto stack = stack_;

   for (const auto& id : stack) {
      const auto* root = world_.FindObjectById(id);
      const auto* canvas = root ? root->GetComponent<UICanvasComponent>() : nullptr;
      const auto* screen = root ? root->GetComponent<UIScreenComponent>() : nullptr;
      const bool invalid = !canvas || !screen || !canvas->IsEnabled() || !screen->IsEnabled();

      if (invalid) {
         CloseScreen(id);
      } else if (finishAnimations && IsClosing(id)) {
         const auto animations = ScreenAnimations(world_, id);
         const bool waiting = std::any_of(animations.begin(), animations.end(), [this](const auto* animation) {
            return animation->IsEnabled() && IsUpdateVisible(animation->GetOwner()) &&
                   animation->IsPlaying(UIAnimationEvent::Hide);
         });

         if (!waiting) {
            CloseScreen(id);
         }
      }
   }

   std::erase_if(screens_, [this](const auto& entry) { return !world_.FindObjectById(entry.first); });

   if (!Widget(captured_)) {
      captured_.clear();
   }
}

void UISystem::UpdatePolicy() {
   gameplayPaused_ = gameplayInputBlocked_ = false;

   for (const auto& id : stack_) {
      const auto* root = world_.FindObjectById(id);
      const auto* screen = root ? root->GetComponent<UIScreenComponent>() : nullptr;

      if (!screen || !screen->IsEnabled()) {
         continue;
      }

      const auto& settings = screen->DescribeSettings();
      gameplayPaused_ = gameplayPaused_ || settings.pausesGameplay;
      gameplayInputBlocked_ = gameplayInputBlocked_ || settings.blocksBelowInput;
   }

   gameplayInputBlocked_ = gameplayInputBlocked_ || gameplayPaused_;
}

std::vector<UIElementLayout> UISystem::BuildLayout(Vector2 viewportSize, bool editing) const {
   auto elements = UILayoutSolver::Build(world_, hierarchyOrder_, stack_, viewportSize, editing);

   for (auto& element : elements) {
      if (IsClosing(element.screenId)) {
         element.interactable = false;
      }
   }

   return elements;
}

void UISystem::BeginFrame(
   Vector2 viewportSize, const UIInputFrame& input, float unscaledDeltaTime, bool runtimeUpdate) {
   if (dispatching_) {
      return;
   }

   AdvanceTransitions(runtimeUpdate);
   ApplyRequests();
   AdvanceTransitions(runtimeUpdate);
   inputLayout_ = BuildLayout(viewportSize);

   if (runtimeUpdate) {
      struct DispatchScope {
         bool& flag;

         explicit DispatchScope(bool& value) : flag(value) { flag = true; }

         ~DispatchScope() { flag = false; }
      } scope(dispatching_);

      DispatchInput(input, unscaledDeltaTime);
   } else {
      captured_.clear();
      inputLatched_ = true;
   }

   ApplyRequests();
   AdvanceTransitions(runtimeUpdate);
   inputLayout_ = BuildLayout(viewportSize);
   const auto screenId = InputScreen();
   EnsureFocus(screenId);
   Object* hit = runtimeUpdate && input.active && input.pointerInside
                    ? UILayoutSolver::HitTest(world_, inputLayout_, input.pointer)
                    : nullptr;
   SetInteraction(runtimeUpdate && input.active ? screenId : std::string{}, hit ? hit->GetEntityId() : std::string{});
   UpdatePolicy();
}

std::string UISystem::InstantiateScreen(const std::string& assetId) {
   return UIScreenAssetSerializer::Instantiate(world_, assetId, lastError_);
}

Object* UISystem::FindElement(const std::string& rootId, const std::string& elementKey) const {
   Object* found = nullptr;

   for (auto* object : world_.GetObjectStore().GetObjects()) {
      const auto* rect = object->GetComponent<UIRectTransformComponent>();

      if (!rect || rect->DescribeSettings().elementKey != elementKey) {
         continue;
      }

      const Object* parent = object;

      while (parent && parent->GetEntityId() != rootId) {
         parent = world_.FindObjectById(parent->GetParentEntityId());
      }

      if (parent) {
         if (found) {
            return nullptr;
         }

         found = object;
      }
   }

   return found;
}

std::vector<std::string> UISystem::Focusable(const std::string& screenId) const {
   std::vector<std::string> result;

   for (const auto& element : inputLayout_) {
      if (element.screenId == screenId && InteractiveWidget(element.objectId)) {
         result.push_back(element.objectId);
      }
   }

   std::stable_sort(result.begin(), result.end(), [this](const auto& a, const auto& b) {
      return Widget(a)->DescribeSettings().navigationOrder < Widget(b)->DescribeSettings().navigationOrder;
   });
   return result;
}

std::string UISystem::InputScreen() const {
   for (auto it = stack_.rbegin(); it != stack_.rend(); ++it) {
      if (IsClosing(*it) || !Focusable(*it).empty()) {
         return *it;
      }

      const auto* root = world_.FindObjectById(*it);
      const auto* screen = root ? root->GetComponent<UIScreenComponent>() : nullptr;

      if (screen && screen->DescribeSettings().blocksBelowInput) {
         return *it;
      }
   }

   return {};
}

void UISystem::EnsureFocus(const std::string& screenId) {
   if (screenId.empty() || IsClosing(screenId)) {
      return;
   }

   const auto focusable = Focusable(screenId);
   auto& focused = screens_[screenId].focused;

   if (std::find(focusable.begin(), focusable.end(), focused) != focusable.end()) {
      return;
   }

   focused = focusable.empty() ? std::string{} : focusable.front();
   const auto* root = world_.FindObjectById(screenId);
   const auto* screen = root ? root->GetComponent<UIScreenComponent>() : nullptr;

   if (screen) {
      if (auto* desired = FindElement(screenId, screen->DescribeSettings().defaultFocusKey);
         desired && std::find(focusable.begin(), focusable.end(), desired->GetEntityId()) != focusable.end()) {
         focused = desired->GetEntityId();
      }
   }
}

void UISystem::SetInteraction(const std::string& screenId, const std::string& hovered) {
   const auto screen = screens_.find(screenId);

   for (const auto& element : inputLayout_) {
      auto* object = world_.FindObjectById(element.objectId);
      auto* widget = object ? GetUIWidget(*object) : nullptr;
      const bool active = widget && InteractiveWidget(element.objectId);
      const bool focused = active && screen != screens_.end() && element.screenId == screenId &&
                           screen->second.focused == element.objectId;
      const bool over = active && element.objectId == hovered;

      if (widget) {
         widget->SetInteraction(focused, over, active && element.objectId == captured_);
      }

      // 操作部品が外された場合も、残ったAnimationへ選択解除を通知する。
      if (auto* animation = object ? object->GetComponent<UIAnimationComponent>() : nullptr;
         animation && animation->IsEnabled()) {
         animation->SetSelected(focused || over);
      }
   }
}

void UISystem::DispatchInput(const UIInputFrame& input, float unscaledDeltaTime) {
   const auto screenId = InputScreen();

   if (!input.active || screenId.empty() || IsClosing(screenId)) {
      captured_.clear();
      inputLatched_ = true;
      return;
   }

   EnsureFocus(screenId);
   auto focusable = Focusable(screenId);

   if (inputLatched_) {
      inputLatched_ = input.pointerDown || input.confirm || input.cancel || input.next || input.previous ||
                      std::abs(input.vertical) > 0.5f || std::abs(input.horizontal) > 0.5f;
      return;
   }

   Object* hit = input.pointerInside ? UILayoutSolver::HitTest(world_, inputLayout_, input.pointer) : nullptr;
   const std::string hitId = hit ? hit->GetEntityId() : std::string{};

   if (hit && (input.pointerMoved || input.pointerPressed)) {
      // 非モーダル画面越しのクリックでも、実際に属する画面の選択を保持する。
      for (const auto& element : inputLayout_) {
         if (element.objectId == hitId) {
            screens_[element.screenId].focused = hitId;
            break;
         }
      }
   }

   if (input.pointerPressed) {
      captured_ = hitId;
   }

   const auto captured = std::find_if(
      inputLayout_.begin(), inputLayout_.end(), [&](const auto& element) { return element.objectId == captured_; });

   if (captured == inputLayout_.end() || !InteractiveWidget(captured_)) {
      captured_.clear();
   } else if (input.pointerDown) {
      // コールバックを呼ぶ前にIDと矩形をコピーし、戻った後は部品をIDから取得し直す。
      const auto element = *captured;
      Vector2 local;

      if (UILayoutSolver::LocalPoint(element, input.pointer, local)) {
         if (auto* widget = InteractiveWidget(element.objectId)) {
            widget->SetNormalizedValue(local.x / element.size.x);
         }
      }
   }

   if (!requests_.empty()) {
      return;
   }

   bool activated = false;

   if (input.pointerReleased) {
      const auto target = std::exchange(captured_, {});

      if (!target.empty() && target == hitId) {
         if (auto* widget = InteractiveWidget(target)) {
            PlayClickAnimation(world_.FindObjectById(target));
            widget->Activate();
            activated = true;
         }
      }
   }

   if (!requests_.empty()) {
      return;
   }

   const auto* root = world_.FindObjectById(screenId);
   const auto* screen = root ? root->GetComponent<UIScreenComponent>() : nullptr;

   if (!screen) {
      return;
   }

   const auto* selected = InteractiveWidget(screens_[screenId].focused);
   const auto adjustmentAxis = selected ? selected->GetAdjustmentAxis() : UIAdjustmentAxis::None;
   const float adjustment = adjustmentAxis == UIAdjustmentAxis::Horizontal ? input.horizontal : input.vertical;
   const bool adjusting = adjustmentAxis != UIAdjustmentAxis::None && std::abs(adjustment) > 0.5f;
   const float axis =
      adjusting ? adjustment : (screen->DescribeSettings().horizontalNavigation ? input.horizontal : input.vertical);
   const float direction = axis > 0.5f ? 1.0f : (axis < -0.5f ? -1.0f : 0.0f);
   bool repeat = false;

   if (direction != heldDirection_) {
      heldDirection_ = direction;
      repeatElapsed_ = -kRepeatDelaySeconds;
      repeat = direction != 0.0f;
   } else if (direction != 0.0f) {
      repeatElapsed_ += std::max(0.0f, unscaledDeltaTime);

      if (repeatElapsed_ >= 0.0f) {
         repeat = true;
         repeatElapsed_ = -kRepeatIntervalSeconds;
      }
   }

   if (input.next || input.previous || repeat) {
      bool consumed = false;

      if (adjusting && !input.next && !input.previous) {
         if (auto* widget = InteractiveWidget(screens_[screenId].focused)) {
            consumed = widget->Adjust(direction);
         }
      }

      if (!requests_.empty()) {
         return;
      }

      if (!consumed && !focusable.empty()) {
         const auto it = std::find(focusable.begin(), focusable.end(), screens_[screenId].focused);
         const auto index = static_cast<std::int32_t>(std::distance(focusable.begin(), it));
         const auto count = static_cast<std::int32_t>(focusable.size());
         const auto step = input.previous ? -1 : (input.next || direction > 0.0f ? 1 : -1);
         screens_[screenId].focused = focusable[static_cast<size_t>((index + step + count) % count)];
      }
   }

   if (input.confirm && !activated) {
      const auto target = screens_[screenId].focused;

      if (auto* widget = InteractiveWidget(target)) {
         PlayClickAnimation(world_.FindObjectById(target));
         widget->Activate();
      }
   }

   if (!requests_.empty()) {
      return;
   }

   if (input.cancel) {
      Pop();
   }
}
}
