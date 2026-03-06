#include "pch.h"
#include "TransformationMatrix.h"
#include "ResourceHelper.h"
#include "GraphicsDevice.h"

namespace GameEngine {
namespace {
GraphicsDevice* sDevice_ = nullptr;
bool sIsInitialized_ = false;
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
   // 今回は赤を書き込む
   transformationMatrixData_->wVP = wvp;
   transformationMatrixData_->world = world;
}
}