#include "pch.h"
#include "VectorOperations.h"

namespace GameEngine {

Vector3 TransformCoordinate(const Vector3& vector, const Matrix4x4& matrix) {
   Vector3 result;
   result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + matrix.m[3][0];
   result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + matrix.m[3][1];
   result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + matrix.m[3][2];
   float w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + matrix.m[3][3];
   result = result / w;
   return result;
}

Vector4 TransformVectorByMatrix(const Vector4& vec, const Matrix4x4& m) {
   return Vector4(
	  vec.x * m.m[0][0] + vec.y * m.m[1][0] + vec.z * m.m[2][0] + vec.w * m.m[3][0],
	  vec.x * m.m[0][1] + vec.y * m.m[1][1] + vec.z * m.m[2][1] + vec.w * m.m[3][1],
	  vec.x * m.m[0][2] + vec.y * m.m[1][2] + vec.z * m.m[2][2] + vec.w * m.m[3][2],
	  vec.x * m.m[0][3] + vec.y * m.m[1][3] + vec.z * m.m[2][3] + vec.w * m.m[3][3]
   );
}

Vector3 TransformPosition(const Matrix4x4& mat, const Vector3& pos) {
   Vector4 v = Vector4(pos.x, pos.y, pos.z, 1.0f);
   Vector4 transformed = v * mat;
   return Vector3(transformed.x / transformed.w,
	  transformed.y / transformed.w,
	  transformed.z / transformed.w);
}

Vector3 TransformNormal(const Vector3& vector, const Matrix4x4& matrix) {
   Vector3 result;
   result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0];
   result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1];
   result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2];
   return result;
}

Vector3 ComputeNormal(const Vector3& a, const Vector3& b, const Vector3& c) {
   Vector3 ab = b - a;
   Vector3 ac = c - a;
   Vector3 n = ac.Cross(ab);
   return n.Normalize();
}

Vector3 Normalize(const Vector3& vec) {
   float length = std::sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
   if (length == 0.0f) {
	  return Vector3(0.0f, 0.0f, 0.0f);
   }
   return Vector3(vec.x / length, vec.y / length, vec.z / length);
}

Vector3 Project(const Vector3& worldPosition, float viewportX, float viewportY, float viewportWidth, float viewportHeight, const Matrix4x4& viewProjectionMatrix) {
   Vector4 clipSpacePosition = TransformVectorByMatrix(Vector4(worldPosition.x, worldPosition.y, worldPosition.z, 1.0f), viewProjectionMatrix);

   if (std::abs(clipSpacePosition.w) < 1e-6f) {
	  return Vector3(-9999.0f, -9999.0f, 1.0f);
   }

   float ndcX = clipSpacePosition.x / clipSpacePosition.w;
   float ndcY = clipSpacePosition.y / clipSpacePosition.w;

   float viewportXPos = (ndcX + 1.0f) * 0.5f * viewportWidth + viewportX;
   float viewportYPos = (1.0f - (ndcY + 1.0f) * 0.5f) * viewportHeight + viewportY;

   return Vector3(viewportXPos, viewportYPos, clipSpacePosition.z / clipSpacePosition.w);
}

Vector2 Cross(const Vector2& a, const Vector2& b) {
   // 2Dベクトルの外積はスカラー値
   return Vector2{0.0f, a.x * b.y - a.y * b.x};
}

} // namespace GameEngine
