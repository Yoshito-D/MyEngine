#include "GameObject.h"

void GameObject::Update(float dt) {
   (void)dt;
   // 基底クラスのデフォルト実装
}

void GameObject::AttachStateMachine() {
   if (!stateMachine_) {
	  stateMachine_ = std::make_unique<StateMachine>();
   }
}
