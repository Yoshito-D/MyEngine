#pragma once

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorSceneContext.h"
#include "GameEngine/Object/Sprite/Sprite.h"
#include "GameEngine/UI/Text/TextTypes.h"
#include "GameEngine/Scene/Camera/Camera.h"
#include "ImGuizmo.h"

// 配置コマンドとビューポート操作で共有する、エディタ内部の座標計算。
namespace GameEngine::EditorSceneGeometry {
constexpr std::int32_t kJsonIndentSize = 3;
constexpr float kVectorLengthEpsilonSquared = 1.0e-8f;
constexpr float kHomogeneousCoordinateEpsilon = 1.0e-6f;
constexpr float kSafeNormalizedDeviceCoordinateLimit = 0.95f;
constexpr float kScreenSpaceFarClip = 100.0f;
constexpr float kDefaultScreenSpaceDepth = 1.0f;
constexpr float kDuplicatePositionOffset = 1.0f;

/// @brief 操作種別をImGuizmoの操作軸へ変換する。
ImGuizmo::OPERATION ToImGuizmoOperation(EditorSceneContext::GizmoOperation operation, bool restrictTo2D = false);

/// @brief 操作空間をImGuizmoの座標系へ変換する。
ImGuizmo::MODE ToImGuizmoMode(EditorSceneContext::GizmoMode mode);

/// @brief 行列を平行移動・回転・拡縮に分解する。
Transform MatrixToTransform(const Matrix4x4& matrix);

/// @brief 二つの値の差の絶対値を求める。
float AbsDiff(float lhs, float rhs);

/// @brief 複製元の名前から複製後の表示名を作成する。
std::string BuildDuplicateName(const std::string& name);

/// @brief ベクトルの全要素が有限値か調べる。
bool IsFiniteVector(const Vector3& value);

/// @brief ベクトルを正規化し、無効な場合は指定した既定値を返す。
Vector3 NormalizeOrFallback(const Vector3& value, const Vector3& fallback);

/// @brief 実際のビュー行列からカメラの位置を取得する。
Vector3 ExtractCameraPositionFromView(const Camera* camera);

/// @brief 実際のビュー行列からカメラの前方を取得する。
Vector3 ExtractCameraForwardFromView(const Camera* camera);

/// @brief 指定位置がカメラの表示範囲内に投影されるか調べる。
bool IsProjectedInsideCamera(const Camera* camera, const Vector3& worldPosition);

/// @brief Objectがスクリーン空間で描画されるか調べる。
bool UsesScreenRenderSpace(const Object* object);

/// @brief ビューポートと出力比率に対応する画面用カメラのサイズを取得する。
Vector2 GetEditorScreenCameraSize(float viewportWidth, float viewportHeight);

/// @brief 画面配置の基準サイズを取得する。
Vector2 GetEditorScreenLayoutSize();

/// @brief 指定した画面アンカーから配置原点の補正量を計算する。
Vector3 CalculateScreenAnchorOffset(Sprite::AnchorPoint anchorPoint, const Vector2& screenSize);

/// @brief 指定した画面アンカーから配置原点の補正量を計算する。
Vector3 CalculateScreenAnchorOffset(UIAnchor anchorPoint, const Vector2& screenSize);

/// @brief Objectの画面アンカーに応じた描画位置の補正量を取得する。
Vector3 GetScreenRenderOffset(const Object* object, const Vector2& screenSize);

/// @brief アンカー相対の座標をギズモ用の画面中心座標へ変換する。
Vector3 ToEditorScreenWorldPosition(const Vector3& screenTranslation, const Vector3& anchorOffset);

/// @brief ギズモ用の画面中心座標をアンカー相対の座標へ戻す。
Vector3 FromEditorScreenWorldPosition(const Vector3& worldPosition, const Vector3& anchorOffset);

/// @brief 画面座標の向きに対応する正射影行列を作成する。
Matrix4x4 MakeScreenSpaceProjectionMatrix(const Vector2& screenSize, bool useDownwardYAxis = false);

/// @brief 画面空間へ配置するときの初期姿勢を作成する。
Transform BuildScreenSpacePlacementTransform();
}

#endif
