#pragma once

namespace GameEngine {
class EmissionModule;
}

namespace ParticleSystemEdit {
/// @brief エミッションモジュール編集ウィンドウを表示する
/// @param emissionModule 編集するエミッションモジュールへのポインタ
void EditEmissionModule(GameEngine::EmissionModule* emissionModule);
}
