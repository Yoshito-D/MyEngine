#pragma once

#include "GameEngine/Graphics/Renderer/DrawCommand.h"
#include <span>

namespace GameEngine {
struct FrameContext;

/// @brief 型付きコマンドをVisitorで受け取り、全描画段階で同じ実行処理を使用する。
class DrawCommandExecutor {
public:
   /// @brief 一件のコマンドを対応する専門レンダラーで実行する。描画先の準備は呼び出し元が行う。
   static void Execute(const DrawCommand& command, FrameContext& context);

   /// @brief 確定した順序のコマンド列を実行する。実行中は元のキューを変更しない。
   static void Execute(std::span<const DrawCommand> commands, FrameContext& context);

private:
   static void ExecuteData(const ModelDrawData& data, FrameContext& context);
   static void ExecuteData(const SpriteDrawData& data, FrameContext& context);
   static void ExecuteData(const UISpriteDrawData& data, FrameContext& context);
   static void ExecuteData(const TextDrawData& data, FrameContext& context);
   static void ExecuteData(const ParticleDrawData& data, FrameContext& context);
   static void ExecuteData(const LineDrawData& data, FrameContext& context);
};
} // namespace GameEngine
