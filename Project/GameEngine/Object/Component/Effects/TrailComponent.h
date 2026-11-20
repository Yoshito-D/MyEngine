#pragma once

#include "GameEngine/Math/VectorMath.h"
#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include <array>
#include <cstdint>
#include <d3d12.h>
#include <deque>
#include <optional>
#include <vector>
#include <wrl.h>

namespace GameEngine {

class Camera;
class GraphicsDevice;

/// @brief オブジェクトの移動履歴をワールド空間に残し、半透明の帯として描画する。
class TrailComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "TrailComponent";               ///< 保存・登録に使う型名。
   static constexpr ComponentDisplayName kDisplayName{"トレイル", "Trail"}; ///< エディター表示名。

   /// @brief コンポーネントの型名を取得する。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 移動と演出状態が確定した後に発生点の座標を記録する。
   ObjectUpdatePhase GetUpdatePhase() const override { return ObjectUpdatePhase::Effects; }

   /// @brief 保存された初期放出設定で履歴を空にして開始する。
   void OnSceneLoaded(SceneWorld& sceneWorld) override;

   /// @brief コンポーネントを無効にしたときは履歴を破棄する。
   void OnDisable() override;

   /// @brief 経過時間（秒）で履歴を減衰させ、放出中は各発生点のワールド位置を記録する。
   void Update(float deltaTime) override;

   /// @brief 新しい軌跡の放出を開始または停止する。停止前の軌跡は時間の経過とともに消える。
   void SetEmitting(bool emitting);

   /// @brief 軌跡の表示長を指定値へ滑らかに近づけ、放出停止時は末端から短くする。
   /// @param length 軌跡に沿った目標の長さ（m）。
   void SetTargetLength(float length);

   /// @brief 透明パスで描画できる線分がある場合、その代表位置（m）を取得する。
   std::optional<Vector3> GetSortPosition() const;

   /// @brief カメラを向く帯を生成して現在のコマンドリストに記録する。
   /// @param device バッファ生成と描画に使うデバイス。
   /// @param camera 透明パスの視点。
   /// @param transformSlot ビュー投影行列のルート定数スロット。
   void Draw(GraphicsDevice& device, const Camera& camera, std::uint32_t transformSlot);

   /// @brief 発生位置と表示設定を保存する。
   nlohmann::json Serialize() const override;

   /// @brief 設定を復元し、以前の実行履歴を破棄する。
   void Deserialize(const nlohmann::json& data) override;

   /// @brief 再利用できる発生位置と表示設定を取得する。
   nlohmann::json GetSharedParameters() const override { return Serialize(); }

   /// @brief 共有された表示設定を適用する。発生位置を変えると履歴を破棄する。
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief 初期放出、幅、寿命、色、発生位置を編集する。
   void DrawInspector() override;
#endif

private:
   static constexpr size_t kMaxPaths = 8;
   static constexpr size_t kMaxPointsPerPath = 128;

   struct Point {
      Vector3 position; ///< 記録したワールド位置（m）。
      float age;        ///< 記録後の時間（秒）。
      bool startsStrip; ///< 前の点と接続しないリボンの始点。
   };

   struct Path {
      std::deque<Point> points;     ///< 時間順の位置履歴。
      bool continues = false;       ///< 次の点を既存の履歴へ接続するか。
      float length = 0.0f;          ///< 長さ制御を使う場合の表示済み区間の合計（m）。
      float retractionSpeed = 0.0f; ///< 放出停止時の長さに応じた末端の移動速度（m/s）。
   };

   void Clear();
   void RecordPoint(Path& path, const Vector3& position);
   static float TrimToLength(Path& path, float limit);
   void ReadSettings(const nlohmann::json& data);

   std::vector<Vector3> offsets_{{}}; ///< 所有オブジェクトのローカル座標系での発生点オフセット（m）。
   std::array<Path, kMaxPaths> paths_;
   Vector4 color_{1.0f, 1.0f, 1.0f, 0.8f};
   float width_ = 0.18f;         ///< 先端の全幅（m）。
   float lifetime_ = 0.4f;       ///< 各点の表示時間（秒）。
   float minDistance_ = 0.08f;   ///< 履歴点を固定する最小間隔（m）。
   float breakDistance_ = 20.0f; ///< 1フレームでこの距離（m）を超えると軌跡を分ける。
   bool emitOnAwake_ = true;
   bool emitting_ = true;
   std::optional<float> targetLength_; ///< 実行時に指定された目標長（m）。
   Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
};

} // namespace GameEngine
