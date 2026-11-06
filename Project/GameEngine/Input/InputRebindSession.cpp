#include "GameEngine/Input/InputRebindSession.h"
#include <utility>

namespace GameEngine {

bool InputRebindSession::Begin(InputRebindTarget target, InputBinding initialBinding, std::uint64_t frameNumber) {
   if (!initialBinding.IsValid()) {
      return false;
   }

   if (target.controlIndex) {
      const auto controls = initialBinding.GetPhysicalControls();

      if (*target.controlIndex >= controls.size() ||
          controls[*target.controlIndex].type != InputPhysicalControl::Type::Key) {
         return false;
      }
   }

   target_ = std::move(target);
   initialBinding_ = std::move(initialBinding);
   candidate_.reset();
   captureAfterFrame_ = frameNumber;
   state_ = InputRebindState::WaitingForInput;
   return true;
}

void InputRebindSession::Update(IInputBackend& backend, std::uint64_t frameNumber, const Apply& apply) {
   if (state_ != InputRebindState::WaitingForInput || frameNumber <= captureAfterFrame_) return;

   if (!apply) {
      Cancel();
      return;
   }

   const auto control = CaptureRebindControl(backend, !target_->controlIndex.has_value());

   if (!control) return;

   auto candidate = initialBinding_.Rebind(*control, target_->controlIndex);

   if (!candidate) return;

   if (apply(*candidate, false)) {
      state_ = InputRebindState::Committed;
   } else {
      // 競合確認中に後続の押下で候補を上書きせず、確認した候補だけを確定する。
      candidate_ = std::move(candidate);
      state_ = InputRebindState::WaitingForConflict;
   }
}

bool InputRebindSession::ConfirmConflict(const Apply& apply) {
   if (state_ != InputRebindState::WaitingForConflict || !candidate_ || !apply) {
      return false;
   }

   if (!apply(*candidate_, true)) {
      return false;
   }

   candidate_.reset();
   state_ = InputRebindState::Committed;
   return true;
}

void InputRebindSession::RejectConflict(std::uint64_t frameNumber) {
   if (state_ != InputRebindState::WaitingForConflict) return;

   candidate_.reset();
   captureAfterFrame_ = frameNumber;
   state_ = InputRebindState::WaitingForInput;
}

void InputRebindSession::Cancel() {
   candidate_.reset();
   target_.reset();
   state_ = InputRebindState::Cancelled;
}

bool InputRebindSession::IsActive() const {
   return state_ == InputRebindState::WaitingForInput || state_ == InputRebindState::WaitingForConflict;
}

} // namespace GameEngine
