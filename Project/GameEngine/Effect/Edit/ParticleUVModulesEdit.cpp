#include "pch.h"
#include "ParticleUVModulesEdit.h"
#include "Effect/Module/UVTransformModule.h"
#include "Effect/Module/TextureSheetAnimationModule.h"

#ifdef USE_IMGUI
#include "../../../externals/imgui/imgui.h"
#endif

namespace {
void EditRandomVector2Control(const char* labelPrefix, GameEngine::RandomVector2& value) {
#ifdef USE_IMGUI
	bool randomize = value.randomize;
	if (ImGui::Checkbox((std::string("Random (ランダム)##") + labelPrefix).c_str(), &randomize)) {
		value.randomize = randomize;
	}

	float minValues[2] = { value.minValue.x, value.minValue.y };
	float maxValues[2] = { value.maxValue.x, value.maxValue.y };
	if (ImGui::DragFloat2((std::string("Min (最小)##") + labelPrefix).c_str(), minValues, 0.01f, -10.0f, 10.0f)) {
		value.minValue = GameEngine::Vector2{minValues[0], minValues[1]};
	}
	if (ImGui::DragFloat2((std::string("Max (最大)##") + labelPrefix).c_str(), maxValues, 0.01f, -10.0f, 10.0f)) {
		value.maxValue = GameEngine::Vector2{maxValues[0], maxValues[1]};
	}
#endif
}
}

namespace ParticleSystemEdit {
void EditUVTransformModule(GameEngine::UVTransformModule* module) {
#ifdef USE_IMGUI
	if (!module) return;

	bool enabled = module->IsEnabled();
	if (ImGui::Checkbox("Enabled (有効)##UVTransformModule", &enabled)) {
		module->SetEnabled(enabled);
	}

	if (!enabled) return;

	const char* modeNames[] = { "Constant (固定)", "Random Between Two Constants (2定数ランダム)", "Start To End (開始から終了)" };

	int scrollMode = static_cast<int>(module->GetScrollMode());
	if (ImGui::Combo("Scroll Mode (スクロールモード)##UVTransform", &scrollMode, modeNames, IM_ARRAYSIZE(modeNames))) {
		module->SetScrollMode(static_cast<GameEngine::UVTransformModule::ValueMode>(scrollMode));
	}
	if (module->GetScrollMode() == GameEngine::UVTransformModule::ValueMode::Constant) {
		auto value = module->GetScrollConstant();
		float v[2] = { value.x, value.y };
		if (ImGui::DragFloat2("Scroll##UVTransform", v, 0.01f, -10.0f, 10.0f)) {
			module->SetScrollConstant(GameEngine::Vector2{v[0], v[1]});
		}
	} else if (module->GetScrollMode() == GameEngine::UVTransformModule::ValueMode::RandomBetweenTwoConstants) {
		auto value = module->GetScrollRandom();
		EditRandomVector2Control("UVScrollRandom", value);
		module->SetScrollRandom(value);
	} else {
		auto start = module->GetScrollCurveStart();
		auto end = module->GetScrollCurveEnd();
		float s[2] = { start.x, start.y };
		float e[2] = { end.x, end.y };
		if (ImGui::DragFloat2("Start Scroll##UVTransform", s, 0.01f, -10.0f, 10.0f)) {
			module->SetScrollCurveStart(GameEngine::Vector2{s[0], s[1]});
		}
		if (ImGui::DragFloat2("End Scroll##UVTransform", e, 0.01f, -10.0f, 10.0f)) {
			module->SetScrollCurveEnd(GameEngine::Vector2{e[0], e[1]});
		}
	}

	int rotationMode = static_cast<int>(module->GetRotationMode());
	if (ImGui::Combo("Rotation Mode (回転モード)##UVTransform", &rotationMode, modeNames, IM_ARRAYSIZE(modeNames))) {
		module->SetRotationMode(static_cast<GameEngine::UVTransformModule::ValueMode>(rotationMode));
	}
	if (module->GetRotationMode() == GameEngine::UVTransformModule::ValueMode::Constant) {
		float value = module->GetRotationConstant();
		if (ImGui::DragFloat("Rotation##UVTransform", &value, 0.01f, -6.28318f, 6.28318f)) {
			module->SetRotationConstant(value);
		}
	} else if (module->GetRotationMode() == GameEngine::UVTransformModule::ValueMode::RandomBetweenTwoConstants) {
		auto value = module->GetRotationRandom();
		bool randomize = value.randomize;
		if (ImGui::Checkbox("Random##UVRotation", &randomize)) {
			value.randomize = randomize;
		}
		float minValue = value.minValue;
		float maxValue = value.maxValue;
		if (ImGui::DragFloat("Min Rotation##UVTransform", &minValue, 0.01f, -6.28318f, 6.28318f)) {
			value.minValue = minValue;
		}
		if (ImGui::DragFloat("Max Rotation##UVTransform", &maxValue, 0.01f, -6.28318f, 6.28318f)) {
			value.maxValue = maxValue;
		}
		module->SetRotationRandom(value);
	} else {
		float start = module->GetRotationCurveStart();
		float end = module->GetRotationCurveEnd();
		if (ImGui::DragFloat("Start Rotation##UVTransform", &start, 0.01f, -6.28318f, 6.28318f)) {
			module->SetRotationCurveStart(start);
		}
		if (ImGui::DragFloat("End Rotation##UVTransform", &end, 0.01f, -6.28318f, 6.28318f)) {
			module->SetRotationCurveEnd(end);
		}
	}

	int scaleMode = static_cast<int>(module->GetScaleMode());
	if (ImGui::Combo("Scale Mode (拡縮モード)##UVTransform", &scaleMode, modeNames, IM_ARRAYSIZE(modeNames))) {
		module->SetScaleMode(static_cast<GameEngine::UVTransformModule::ValueMode>(scaleMode));
	}
	if (module->GetScaleMode() == GameEngine::UVTransformModule::ValueMode::Constant) {
		auto value = module->GetScaleConstant();
		float v[2] = { value.x, value.y };
		if (ImGui::DragFloat2("Scale##UVTransform", v, 0.01f, 0.0f, 10.0f)) {
			module->SetScaleConstant(GameEngine::Vector2{v[0], v[1]});
		}
	} else if (module->GetScaleMode() == GameEngine::UVTransformModule::ValueMode::RandomBetweenTwoConstants) {
		auto value = module->GetScaleRandom();
		EditRandomVector2Control("UVScaleRandom", value);
		module->SetScaleRandom(value);
	} else {
		auto start = module->GetScaleCurveStart();
		auto end = module->GetScaleCurveEnd();
		float s[2] = { start.x, start.y };
		float e[2] = { end.x, end.y };
		if (ImGui::DragFloat2("Start Scale##UVTransform", s, 0.01f, 0.0f, 10.0f)) {
			module->SetScaleCurveStart(GameEngine::Vector2{s[0], s[1]});
		}
		if (ImGui::DragFloat2("End Scale##UVTransform", e, 0.01f, 0.0f, 10.0f)) {
			module->SetScaleCurveEnd(GameEngine::Vector2{e[0], e[1]});
		}
	}
#endif
}

void EditTextureSheetAnimationModule(GameEngine::TextureSheetAnimationModule* module) {
#ifdef USE_IMGUI
	if (!module) return;

	bool enabled = module->IsEnabled();
	if (ImGui::Checkbox("Enabled (有効)##TextureSheetAnimation", &enabled)) {
		module->SetEnabled(enabled);
	}

	if (!enabled) return;

	int tilesX = static_cast<int>(module->GetTilesX());
	int tilesY = static_cast<int>(module->GetTilesY());
	if (ImGui::DragInt("Tiles X (横分割数)##TextureSheetAnimation", &tilesX, 1.0f, 1, 64)) {
		module->SetTilesX(static_cast<uint32_t>(tilesX));
	}
	if (ImGui::DragInt("Tiles Y (縦分割数)##TextureSheetAnimation", &tilesY, 1.0f, 1, 64)) {
		module->SetTilesY(static_cast<uint32_t>(tilesY));
	}

	float frameOverTime = module->GetFrameOverTime();
	if (ImGui::DragFloat("Frame Over Time (時間あたりフレーム)##TextureSheetAnimation", &frameOverTime, 0.1f, 0.0f, 100.0f)) {
		module->SetFrameOverTime(frameOverTime);
	}

	int cycles = static_cast<int>(module->GetCycles());
	if (ImGui::DragInt("Cycles##TextureSheetAnimation", &cycles, 1.0f, 1, 100)) {
		module->SetCycles(static_cast<uint32_t>(cycles));
	}

	int frameCount = static_cast<int>(module->GetFrameCount());
	int maxFrameCount = static_cast<int>(module->GetTilesX() * module->GetTilesY());
	if (module->GetAnimationMode() == GameEngine::TextureSheetAnimationModule::AnimationMode::SingleRow) {
		maxFrameCount = static_cast<int>(module->GetTilesX());
	}
	if (ImGui::DragInt("Frame Count (0=All)##TextureSheetAnimation", &frameCount, 1.0f, 0, maxFrameCount)) {
		module->SetFrameCount(static_cast<uint32_t>(frameCount));
	}

	int animationMode = static_cast<int>(module->GetAnimationMode());
	const char* modeNames[] = { "Whole Sheet", "Single Row" };
	if (ImGui::Combo("Animation Mode##TextureSheetAnimation", &animationMode, modeNames, IM_ARRAYSIZE(modeNames))) {
		module->SetAnimationMode(static_cast<GameEngine::TextureSheetAnimationModule::AnimationMode>(animationMode));
		if (module->GetAnimationMode() == GameEngine::TextureSheetAnimationModule::AnimationMode::WholeSheet) {
			module->SetRandomRow(false);
		}
	}

	int startFrame = static_cast<int>(module->GetStartFrame());
	if (ImGui::DragInt("Start Frame##TextureSheetAnimation", &startFrame, 1.0f, 0, 1024)) {
		module->SetStartFrame(static_cast<uint32_t>(startFrame));
	}

	if (module->GetAnimationMode() == GameEngine::TextureSheetAnimationModule::AnimationMode::SingleRow) {
		bool randomRow = module->GetRandomRow();
		if (ImGui::Checkbox("Random Row##TextureSheetAnimation", &randomRow)) {
			module->SetRandomRow(randomRow);
		}

		if (!randomRow) {
			int rowIndex = static_cast<int>(module->GetRowIndex());
			if (ImGui::DragInt("Row Index##TextureSheetAnimation", &rowIndex, 1.0f, 0, 1024)) {
				module->SetRowIndex(static_cast<uint32_t>(rowIndex));
			}
		}
	}
#endif
}
}
