#pragma once
#include <string_view>

namespace GameEngine {
struct FrameContext;

/// @brief レンダーパスの種別
enum class RenderPassType {
	Opaque,       ///< 不透明オブジェクト（ポストプロセス前）
	Transparent,  ///< 半透明オブジェクト（ポストプロセス前）
	PostEffect,   ///< ポストエフェクト + ポストプロセス後描画
};

/// @brief レンダーパスインターフェース
/// Renderer::EndFrame() がこのインターフェース経由でパスを実行する。
/// 将来のパス追加は IRenderPass を実装するだけでよい。
class IRenderPass {
public:
	virtual ~IRenderPass() = default;

	/// @brief パスを実行する
	/// @param ctx 今フレームのフレームコンテキスト
	virtual void Execute(FrameContext& ctx) = 0;

	/// @brief パス名を返す（デバッグ / ImGui 表示用）
	virtual std::string_view GetName() const = 0;

	/// @brief パス種別を返す
	virtual RenderPassType GetPassType() const = 0;
};

} // namespace GameEngine
