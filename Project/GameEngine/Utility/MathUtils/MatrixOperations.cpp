#include "pch.h"
#include "MatrixOperations.h"
#include <numbers>

namespace GameEngine {

float ToRadians(float degrees) {
   return degrees * std::numbers::pi_v<float> / 180.0f;
}

float ToDegrees(float radians) {
   return radians * 180.0f / std::numbers::pi_v<float>;
}

Matrix4x4 MakeIdentity4x4() {
   Matrix4x4 matrix = {
	   1.0f,0.0f,0.0f,0.0f,
	   0.0f,1.0f,0.0f,0.0f,
	   0.0f,0.0f,1.0f,0.0f,
	   0.0f,0.0f,0.0f,1.0f
   };
   return matrix;
}

Matrix3x3 MakeIdentity3x3() {
   Matrix3x3 matrix = {
	   1.0f,0.0f,0.0f,
	   0.0f,1.0f,0.0f,
	   0.0f,0.0f,1.0f
   };
   return matrix;
}

Transform TransformInitialize() {
   Transform transform;
   transform.translation = Vector3(0.0f, 0.0f, 0.0f);
   transform.rotation = Vector3(0.0f, 0.0f, 0.0f);
   transform.scale = Vector3(1.0f, 1.0f, 1.0f);
   return transform;
}

Matrix4x4 MakeScaleMatrix(const Vector3& scale) {
   Matrix4x4 result = {
	   scale.x,0.0f,0.0f,0.0f,
	   0.0f,scale.y,0.0f,0.0f,
	   0.0f,0.0f,scale.z,0.0f,
	   0.0f,0.0f,0.0f,1.0f
   };
   return result;
}

Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
   Matrix4x4 result = {
	   1.0f,0.0f,0.0f,0.0f,
	   0.0f,1.0f,0.0f,0.0f,
	   0.0f,0.0f,1.0f,0.0f,
	   translate.x,translate.y,translate.z,1.0f
   };
   return result;
}

Matrix4x4 MakeRotateXMatrix(float radian) {
   Matrix4x4 result = {
	   1.0f,0.0f,0.0f,0.0f,
	   0.0f,std::cos(radian),std::sin(radian),0.0f,
	   0.0f,-std::sin(radian),std::cos(radian),0.0f,
	   0.0f,0.0f,0.0f,1.0f
   };
   return result;
}

Matrix4x4 MakeRotateYMatrix(float radian) {
   Matrix4x4 result = {
	   std::cos(radian),0.0f,-std::sin(radian),0.0f,
	   0.0f,1.0f,0.0f,0.0f,
	   std::sin(radian),0.0f,std::cos(radian),0.0f,
	   0.0f,0.0f,0.0f,1.0f
   };
   return result;
}

Matrix4x4 MakeRotateZMatrix(float radian) {
   Matrix4x4 result = {
	   std::cos(radian),std::sin(radian),0.0f,0.0f,
	   -std::sin(radian),std::cos(radian),0.0f,0.0f,
	   0.0f,0.0f,1.0f,0.0f,
	   0.0f,0.0f,0.0f,1.0f
   };
   return result;
}

Matrix4x4 MakeAffineMatrix(const Transform& transform) {
   Matrix4x4 rotateXYZMatrix = MakeRotateXMatrix(transform.rotation.x) * MakeRotateYMatrix(transform.rotation.y) * MakeRotateZMatrix(transform.rotation.z);
   Matrix4x4 result = {
	   transform.scale.x * rotateXYZMatrix.m[0][0],transform.scale.x * rotateXYZMatrix.m[0][1],transform.scale.x * rotateXYZMatrix.m[0][2],0.0f,
	   transform.scale.y * rotateXYZMatrix.m[1][0],transform.scale.y * rotateXYZMatrix.m[1][1],transform.scale.y * rotateXYZMatrix.m[1][2],0.0f,
	   transform.scale.z * rotateXYZMatrix.m[2][0],transform.scale.z * rotateXYZMatrix.m[2][1],transform.scale.z * rotateXYZMatrix.m[2][2],0.0f,
	   transform.translation.x,transform.translation.y,transform.translation.z,1.0f
   };
   return result;
}

Matrix4x4 MakeRotationAxis(const Vector3& axis, float angle) {
   Vector3 a = axis.Normalize();
   float x = a.x;
   float y = a.y;
   float z = a.z;

   float c = std::cos(angle);
   float s = std::sin(angle);
   float t = 1.0f - c;

   Matrix4x4 result;
   result.m[0][0] = t * x * x + c;
   result.m[0][1] = t * x * y - s * z;
   result.m[0][2] = t * x * z + s * y;
   result.m[0][3] = 0.0f;

   result.m[1][0] = t * x * y + s * z;
   result.m[1][1] = t * y * y + c;
   result.m[1][2] = t * y * z - s * x;
   result.m[1][3] = 0.0f;

   result.m[2][0] = t * x * z - s * y;
   result.m[2][1] = t * y * z + s * x;
   result.m[2][2] = t * z * z + c;
   result.m[2][3] = 0.0f;

   result.m[3][0] = 0.0f;
   result.m[3][1] = 0.0f;
   result.m[3][2] = 0.0f;
   result.m[3][3] = 1.0f;

   return result;
}

Matrix4x4 MakeRotateAxisAngle(const Vector3& axis, float angle) {
   Vector3 n = axis.Normalize();
   float x = n.x;
   float y = n.y;
   float z = n.z;
   float c = std::cos(angle);
   float s = std::sin(angle);
   float t = 1.0f - c;

   Matrix4x4 result = {
	   t * x * x + c,        t * x * y + s * z,    t * x * z - s * y,    0.0f,
	   t * x * y - s * z,    t * y * y + c,        t * y * z + s * x,    0.0f,
	   t * x * z + s * y,    t * y * z - s * x,    t * z * z + c,        0.0f,
	   0.0f,                 0.0f,                 0.0f,                 1.0f
   };
   return result;
}

Matrix4x4 MakeRotationMatrixFromTo(const Vector3& from, const Vector3& to) {
   Vector3 f = from.Normalize();
   Vector3 t = to.Normalize();

   float cosTheta = f.Dot(t);
   const float EPSILON = 1e-6f;

   // 同じ方向
   if (cosTheta > 1.0f - EPSILON) {
	  return MakeIdentity4x4();
   }
   // 反対方向（180度回転）
   if (cosTheta < -1.0f + EPSILON) {
	  // from と直交する適当な軸を回転軸にする（ここではX軸基準）
	  Vector3 orthogonal = from.Cross(Vector3(1.0f, 0.0f, 0.0f));
	  if (orthogonal.LengthSquared() < EPSILON) {
		 orthogonal = from.Cross(Vector3(0.0f, 1.0f, 0.0f)); // fallback
	  }
	  orthogonal.Normalize();
	  return MakeRotationAxis(orthogonal, DirectX::XM_PI); // πラジアン = 180度
   }

   Vector3 axis = f.Cross(t).Normalize();
   float angle = std::acos(cosTheta);
   return MakeRotationAxis(axis, angle);
}

Matrix4x4 DirectionToDirection(const Vector3& from, const Vector3& to) {
   Vector3 f = from.Normalize();
   Vector3 t = to.Normalize();

   float cosTheta = std::clamp(f.Dot(t), -1.0f, 1.0f);

   if (cosTheta > 0.999999f) {
	  return Matrix4x4::Identity();
   }

   if (cosTheta < -0.999999f) {
	  Vector3 orthogonal;
	  if (fabsf(f.x) < fabsf(f.y) && fabsf(f.x) < fabsf(f.z)) {
		 orthogonal = Vector3{ 1.0f, 0.0f, 0.0f };
	  } else if (fabsf(f.y) < fabsf(f.z)) {
		 orthogonal = Vector3{ 0.0f, 1.0f, 0.0f };
	  } else {
		 orthogonal = Vector3{ 0.0f, 0.0f, 1.0f };
	  }

	  Vector3 axis = f.Cross(orthogonal).Normalize();
	  return MakeRotateAxisAngle(axis, DirectX::XM_PI);
   }

   Vector3 axis = f.Cross(t).Normalize();
   float angle = acosf(cosTheta);
   return MakeRotateAxisAngle(axis, angle);
}

Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
   Matrix4x4 result = {
	   1.0f / aspectRatio * (std::cos(fovY * 0.5f) / std::sin(fovY * 0.5f)),0.0f,0.0f,0.0f,
	   0.0f,std::cos(fovY * 0.5f) / std::sin(fovY * 0.5f),0.0f,0.0f,
	   0.0f,0.0f,farClip / (farClip - nearClip),1.0f,
	   0.0f,0.0f,(-nearClip * farClip) / (farClip - nearClip),0.0f
   };
   return result;
}

Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
   Matrix4x4 result = {
	   2.0f / (right - left),0.0f,0.0f,0.0f,
	   0.0f,2.0f / (top - bottom),0.0f,0.0f,
	   0.0f,0.0f,1.0f / (farClip - nearClip),0.0f,
	   (left + right) / (left - right),(top + bottom) / (bottom - top),nearClip / (nearClip - farClip),1.0f
   };
   return result;
}

Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
   Matrix4x4 result = {
	   width * 0.5f,0.0f,0.0f,0.0f,
	   0.0f,-height * 0.5f,0.0f,0.0f,
	   0.0f,0.0f,maxDepth - minDepth,0.0f,
	   left + width * 0.5f,top + height * 0.5f,minDepth,1.0f
   };
   return result;
}

Matrix4x4 MakeLookAtMatrix(const Vector3& eye, const Vector3& target, const Vector3& up) {
   Vector3 zaxis = (target - eye).Normalize();
   Vector3 xaxis = (up.Cross(zaxis)).Normalize();
   Vector3 yaxis = zaxis.Cross(xaxis);

   Matrix4x4 result;
   result.m[0][0] = xaxis.x;
   result.m[1][0] = xaxis.y;
   result.m[2][0] = xaxis.z;
   result.m[3][0] = -xaxis.Dot(eye);

   result.m[0][1] = yaxis.x;
   result.m[1][1] = yaxis.y;
   result.m[2][1] = yaxis.z;
   result.m[3][1] = -yaxis.Dot(eye);

   result.m[0][2] = zaxis.x;
   result.m[1][2] = zaxis.y;
   result.m[2][2] = zaxis.z;
   result.m[3][2] = -zaxis.Dot(eye);

   result.m[0][3] = 0.0f;
   result.m[1][3] = 0.0f;
   result.m[2][3] = 0.0f;
   result.m[3][3] = 1.0f;

   return result;
}

Matrix4x4 ExtractRotationMatrix(const Matrix4x4& worldMatrix) {
   Matrix4x4 rotationOnly = worldMatrix;
   rotationOnly.m[3][0] = 0.0f;
   rotationOnly.m[3][1] = 0.0f;
   rotationOnly.m[3][2] = 0.0f;
   rotationOnly.m[3][3] = 1.0f;
   return rotationOnly;
}

Vector3 MatrixToEulerXYZ(const Matrix4x4& m) {
   float roll, pitch, yaw;

   if (fabs(m.m[2][1]) < 1.0f) {
	  pitch = asin(m.m[2][1]);
	  roll = atan2(-m.m[2][0], m.m[2][2]);
	  yaw = atan2(-m.m[0][1], m.m[1][1]);
   } else {
	  pitch = (m.m[2][1] > 0) ? DirectX::XM_PI * 0.5f : -DirectX::XM_PI * 0.5f;
	  roll = 0.0f;
	  yaw = atan2(m.m[1][0], m.m[0][0]);
   }

   return { roll, pitch, yaw };
}

Vector3 ExtractYawPitchRoll(const Matrix4x4& m) {
   Vector3 angles{};

   // forward ベクトル（Z軸）
   Vector3 forward{ m.m[2][0], m.m[2][1], m.m[2][2] };

   // yaw (Y回転)
   angles.y = std::atan2(forward.x, forward.z);

   // pitch (X回転)
   float len = std::sqrt(forward.x * forward.x + forward.z * forward.z);
   angles.x = std::atan2(-forward.y, len);

   // roll (Z回転) は Up ベクトルから計算する
   Vector3 up{ m.m[1][0], m.m[1][1], m.m[1][2] };
   angles.z = std::atan2(m.m[0][1], m.m[1][1]);

   return angles;
}

} // namespace GameEngine
