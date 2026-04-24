#include "pch.h"
#include "DebugCamera.h"
#include <EngineContext.h>

namespace GameEngine {

void DebugCamera::Initialize(const CameraState& initialState) {
	VirtualCamera::Initialize(initialState);

	// OrbitalBodyコンポーネントを追加
	orbitalBody_ = AddComponent<OrbitalBody>();

	VirtualCamera::SetName("DebugCamera");
}

void DebugCamera::Update(float deltaTime) {
	// シーンがホバーされている時のみ入力を処理
#ifdef USE_IMGUI
	if (EngineContext::GetIsSceneHovered()) {
		Vector2 mouseDelta = EngineContext::GetMouseDelta();
		int32_t wheel = EngineContext::GetMouseWheelDelta();
		bool isDragging = EngineContext::IsMousePressed(2);
		bool isShiftPressed = EngineContext::IsKeyPressed(DIK_LSHIFT);

		if (orbitalBody_) {
			orbitalBody_->ProcessInput(mouseDelta, wheel, isDragging, isShiftPressed);
		}
	}
#endif

	// 基底クラスの更新（コンポーネントを実行してカメラ状態を計算）
	VirtualCamera::Update(deltaTime);
}

void DebugCamera::SetDistance(float distance) {
	if (orbitalBody_) {
		orbitalBody_->SetDistance(distance);
	}
}

} // namespace GameEngine
