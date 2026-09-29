#pragma once
#ifdef USE_IMGUI
#include <string>
namespace GameEngine {
/// @brief Draw the shared audio selector and drop target; true only when the asset ID changed.
bool DrawAudioAssetWidget(const char* label, std::string& assetId);
}
#endif
