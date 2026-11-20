#pragma once

#include <cstdint>
#include <d3d12.h>
#include <functional>
#include <optional>
#include <variant>
#include <vector>
#include "GameEngine/Graphics/Pipeline/PipelineState.h"
#include "GameEngine/Object/Sprite/Sprite.h"

namespace GameEngine {
class Model;
class ParticleSystem;
class TrailComponent;
class Texture;
class Camera;
class RenderQueue;

/// @brief コマンドを実行する描画段階。Countはキューの配列サイズに使用する。
enum class RenderPass : std::uint32_t {
   Opaque,      ///< 不透明オブジェクト（ポストプロセス前）。
   Transparent, ///< 半透明オブジェクト（ポストプロセス前）。
   PostProcess, ///< ポストプロセス後。
   Overlay,     ///< 背景の画面効果も適用した後の共通UI。
   Count
};

/// @brief 距離が同じ、または距離を持たない描画の優先度。大きい値を先に描画する。
enum class DrawSortPriority : std::int32_t {
   Line = 1,
   Particle,
   SpriteAndText,
   Model
};

/// @brief 一つのサブメッシュの描画データ。参照先はフレームの描画完了まで生存させる。
struct ModelDrawData {
   Model* model = nullptr;                                    ///< 描画対象への非所有参照。
   size_t meshIndex = 0;                                      ///< 生成器で分割済みのサブメッシュ番号。
   std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> textures;         ///< メッシュ順に対応するSRVハンドルの所有配列。
   D3D12_GPU_DESCRIPTOR_HANDLE environmentTextureSrvHandle{}; ///< 環境テクスチャSRV。ptrが0なら未指定。
   Camera* camera = nullptr;                                  ///< 描画時のカメラへの非所有参照。
   BlendMode blendMode = BlendMode::kBlendModeNormal;         ///< スロットとパイプラインの設定から解決済みのブレンド。
};

/// @brief ワールド空間スプライトの描画データ。
struct SpriteDrawData {
   Sprite* sprite = nullptr;                          ///< 描画対象への非所有参照。
   Texture* texture = nullptr;                        ///< 描画するテクスチャへの非所有参照。
   D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle{};    ///< 登録時点で確定したSRVハンドル。
   Camera* camera = nullptr;                          ///< 描画時のカメラへの非所有参照。
   BlendMode blendMode = BlendMode::kBlendModeNormal; ///< マテリアルと引数から解決したブレンド。
};

/// @brief 座標変換を登録時に済ませたUIスプライトの描画データ。
struct UISpriteDrawData {
   Sprite* sprite = nullptr;                          ///< 描画対象への非所有参照。
   Texture* texture = nullptr;                        ///< 描画するテクスチャへの非所有参照。
   D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle{};    ///< 登録時点で確定したSRVハンドル。
   BlendMode blendMode = BlendMode::kBlendModeNormal; ///< 描画時のブレンド方式。
};

/// @brief フォントまたは共通UIの共有ジオメトリを描画する範囲。
struct TextDrawData {
   D3D12_GPU_DESCRIPTOR_HANDLE atlasSrv{}; ///< フォントアトラスなどのSRVハンドル。
   std::uint32_t indexCount = 0;           ///< 描画に使用する頂点参照番号の数。
   std::uint32_t startIndex = 0;           ///< 共有インデックスバッファ内の開始位置。
   std::int32_t baseVertex = 0;            ///< 頂点の基準オフセット。
   std::int32_t sortingOrder = 0;          ///< UI要素を前後に並べる描画順。
};

/// @brief 粒子のGPU更新と描画に使うデータ。
struct ParticleDrawData {
   ParticleSystem* particleSystem = nullptr; ///< 更新と描画の対象への非所有参照。
   Camera* camera = nullptr;                 ///< 描画時のカメラへの非所有参照。
};

/// @brief 透明パスで描画するオブジェクトの軌跡と、登録時点の位置（m）。
struct TrailDrawData {
   TrailComponent* trail = nullptr; ///< フレーム中の描画対象への非所有参照。
   Camera* camera = nullptr;         ///< 描画時の視点への非所有参照。
   Vector3 sortPosition{};
};

/// @brief 線の描画関数と、登録時点で確定した視点。
struct LineDrawData {
   std::function<void(ID3D12GraphicsCommandList*, const Matrix4x4&)> drawFunc; ///< 行列を受け取る描画関数。
   Camera* camera = nullptr;                                                   ///< ソートに使うカメラへの非所有参照。
   Matrix4x4 viewProjectionMatrix{};    ///< 登録時のViewProjection行列のスナップショット。
   std::optional<Vector3> sortPosition; ///< 透明ソート用の代表位置（m）。
};

/// @brief 一つのコマンドが保持する型付きデータ。種別タグと実データの不一致を防ぐ。
using DrawPayload =
   std::variant<ModelDrawData, SpriteDrawData, UISpriteDrawData, TextDrawData, ParticleDrawData, TrailDrawData,
      LineDrawData>;

/// @brief コマンドの型に依存せず比較できる透明ソート情報。
struct DrawSortData {
   std::optional<Vector3> position;                    ///< 距離ソートに使う位置（m）。画面空間UIなどは未指定。
   Camera* camera = nullptr;                           ///< 距離を計算するカメラへの非所有参照。
   DrawSortPriority priority = DrawSortPriority::Line; ///< 距離情報がないか距離が等しい場合の描画優先度。
};

/// @brief キューが値として所有する一回分の描画要求。GPU実行はVisitorへ委譲する。
class DrawCommand {
public:
   /// @brief 実行段階と型付きデータを所有し、ソート情報を初期化する。
   /// @param renderPass コマンドの実行段階。
   /// @param payload フレーム終了まで有効な参照と、コマンド自身が保持する描画データ。
   DrawCommand(RenderPass renderPass, DrawPayload payload);

   /// @brief このコマンドを実行する描画段階を返す。
   RenderPass GetRenderPass() const { return renderPass_; }

   /// @brief Visitorへ渡す型付きデータを取得する。
   const DrawPayload& GetPayload() const { return payload_; }

   /// @brief 登録時または直近のソート直前に確定した比較情報を返す。
   const DrawSortData& GetSortData() const { return sortData_; }

private:
   friend class RenderQueue;
   void RefreshSortData();
   RenderPass renderPass_; ///< コマンドが所属する描画段階。
   DrawPayload payload_;   ///< 実行時に使用する唯一の具象データ。
   DrawSortData sortData_; ///< ソート処理が共通に参照する比較情報。
};
} // namespace GameEngine
