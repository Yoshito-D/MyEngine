#pragma once  
#include <vector>  
#include <wrl.h>  
#include <d3d12.h>  
#include "Line.h" 
#include "Core/Graphics/TransformationMatrix.h"  
#include <unordered_map>

using namespace Microsoft::WRL;

namespace GameEngine {
class Line;
class Camera;

/// @brief グリッド描画の平面タイプ
enum class GridPlane {
   XZ,  // XZ平面（Y軸が上）
   XY,  // XY平面（Z軸が上）
   YZ   // YZ平面（X軸が上）
};

class LineRenderer {
public:
   struct LineInstance {
	  Vector3 start;
	  Vector3 end;
	  Vector4 color;
   };

   void Initialize(ID3D12Device* device, size_t maxLines);
   void Begin();
   void End();
   void UpdateMatrix(const Matrix4x4& world, const Matrix4x4& viewProj);
   void Draw(ID3D12GraphicsCommandList* cmdList);

   /// @brief 単純な線を描画する（カメラを指定）
   /// @param start 開始点
   /// @param end 終了点
   /// @param color 色
   /// @param camera カメラ（描画コマンドに登録）
   void DrawLine(const Vector3& start, const Vector3& end, const Vector4& color, Camera* camera);

   /// @brief スプライン曲線を描画する
   /// @param controlPoints 制御点のリスト（最低4つ必要）
   /// @param color 色
   /// @param segmentCount セグメント数（デフォルト：10）
   /// @param camera カメラ（描画コマンドに登録）
   void DrawSpline(const std::vector<Vector3>& controlPoints, const Vector4& color, size_t segmentCount, Camera* camera);

   /// @brief グリッドを描画する
   /// @param camera アクティブカメラ（カメラ位置に基づいてグリッドを生成し、描画コマンドに登録）
   /// @param plane 描画する平面（デフォルト：XZ平面）
   /// @param gridSize グリッドの間隔（デフォルト：1.0f）
   /// @param thickLineInterval 太い線を描画する間隔（デフォルト：10本ごと）
   /// @param range カメラからの範囲（グリッド数、デフォルト：100）
   /// @param enableFade カメラからの距離に応じてフェードアウトするか（デフォルト：true）
   /// @param fadeDistance フェードアウトを開始する距離（デフォルト：50.0f、enableFadeがtrueの場合のみ有効）
   void DrawGrid(Camera* camera, GridPlane plane = GridPlane::XZ, float gridSize = 1.0f, int thickLineInterval = 10, int range = 100, bool enableFade = true, float fadeDistance = 50.0f);

   /// @brief 球を描画する
   /// @param center 中心
   /// @param radius 半径
   /// @param color 色
   /// @param camera カメラ（描画コマンドに登録）
   void DrawSphere(const Vector3& center, float radius, const Vector4& color, Camera* camera);

   /// @brief 半球を描画する
   /// @param center 中心
   /// @param radius 半径
   /// @param up 上方向
   /// @param color 色
   /// @param camera カメラ（描画コマンドに登録）
   void DrawHemisphere(const Vector3& center, float radius, const Vector3& up, const Vector4& color, Camera* camera);

   /// @brief 円錐を描画する
   /// @param apex 円錐の頂点
   /// @param radius 円周の半径
   /// @param height 円錐の高さ
   /// @param direction 円錐の向き
   /// @param color 色
   /// @param camera カメラ（描画コマンドに登録）
   void DrawCone(const Vector3& apex, float radius, float height, const Vector3& direction, const Vector4& color, Camera* camera);

   /// @brief ボックスを描画する
   /// @param center 中心
   /// @param size サイズ
   /// @param color 色
   /// @param camera カメラ（描画コマンドに登録）
   void DrawBox(const Vector3& center, const Vector3& size, const Vector4& color, Camera* camera);

   /// @brief 円を描画する
   /// @param center 中心
   /// @param radius 半径
   /// @param normal 法線
   /// @param color 色
   /// @param camera カメラ（描画コマンドに登録）
   void DrawCircle(const Vector3& center, float radius, const Vector3& normal, const Vector4& color, Camera* camera);

   /// @brief ラインが空かどうかを判定
   /// @return ラインがない場合はtrue
   bool IsEmpty() const { return cameraLineGroups_.empty(); }

   /// @brief カメラごとのライン描画情報を取得
   /// @return カメラとラインデータのマップ
   const std::unordered_map<Camera*, std::vector<LineInstance>>& GetCameraLineGroups() const { return cameraLineGroups_; }

   /// @brief マップされたインスタンスバッファへのポインタを取得
   /// @return マップされたバッファへのポインタ
   UINT8* GetMappedInstanceBuffer() const { return mappedInstanceBuffer_; }

   /// @brief 現在のラインをクリア
   void Clear();

private:
   // カメラごとにラインをグループ化
   std::unordered_map<Camera*, std::vector<LineInstance>> cameraLineGroups_;
   size_t maxLines_ = 0;

   ComPtr<ID3D12Resource> instanceBuffer_;
   D3D12_VERTEX_BUFFER_VIEW instanceVBView_{};
   UINT8* mappedInstanceBuffer_ = nullptr;

   // 基本線分の頂点（インデックス0と1の2点）
   ComPtr<ID3D12Resource> baseVertexBuffer_;
   D3D12_VERTEX_BUFFER_VIEW baseVBView_{};

   TransformationMatrix transformationMatrix_{};

   /// @brief 指定カメラのラインを追加
   void AddLine(Camera* camera, const Line& line);
};
}