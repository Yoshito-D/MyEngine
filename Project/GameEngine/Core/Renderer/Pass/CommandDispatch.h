#pragma once
#include "DrawCommand.h"

namespace GameEngine {
struct FrameContext;

/// @brief 1つの描画コマンドを FrameContext の適切なレンダラーへディスパッチする
/// OpaquePass / TransparentPass / PostEffectPass で共通して使用する。
/// switch による型分岐をここに集約し、各パスはシンプルなループだけを保持する。
void DispatchDrawCommand(const DrawCommand& cmd, FrameContext& ctx);

} // namespace GameEngine
