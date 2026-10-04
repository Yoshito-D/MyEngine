#include "GameEngine/pch.h"
#include "GameEngine/Graphics/Resources/TransformationMatrix.h"
#include "GameEngine/Graphics/Device/ResourceHelper.h"
#include "GameEngine/Graphics/Device/GraphicsDevice.h"

namespace GameEngine {
namespace {
GraphicsDevice* sDevice_ = nullptr;
bool sIsInitialized_ = false;
bool IsFiniteMatrix(const Matrix4x4& matrix) {
   for (const auto& row : matrix.m) for (float element : row) if (!std::isfinite(element)) return false;
   return true;
}
Matrix4x4 NormalMatrix(const Matrix4x4& world) {
   const auto inverseTranspose = world.Inverse().Transpose();
   // UIの零スケールによる非表示にも対応し、退化した面の法線でNaNをGPUへ渡さない。
   return IsFiniteMatrix(inverseTranspose) ? inverseTranspose : MakeIdentity4x4();
}
}

void TransformationMatrix::Initialize(GraphicsDevice* device) {
   if (sIsInitialized_) return;
   sDevice_ = device;
   sIsInitialized_ = true;
}

void TransformationMatrix::Create(const Matrix4x4& wvp, const Matrix4x4& world) {
   if (!sIsInitialized_)return;
   transformationMatrixResource_ = ResourceHelper::CreateBufferResource(sDevice_->GetDevice(), sizeof(TransformationMatrixData));
   // 書き込むためのアドレスを取得
   transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));
   *transformationMatrixData_ = { MakeIdentity4x4(), MakeIdentity4x4(), MakeIdentity4x4() };
   if (IsFiniteMatrix(wvp) && IsFiniteMatrix(world)) *transformationMatrixData_ = { wvp, world, NormalMatrix(world) };
}
void TransformationMatrix::ApplyWorldTransform(const Matrix4x4& world, const Matrix4x4& viewProjection) {
   if (!transformationMatrixData_) return;
   const auto wvp = world * viewProjection;
   if (!IsFiniteMatrix(world) || !IsFiniteMatrix(wvp)) return;
   *transformationMatrixData_ = { wvp, world, NormalMatrix(world) };
}

}