#include "pch.h"
#include "ParticleMaterial.h"
#include "GraphicsDevice.h"
#include "ResourceHelper.h"

namespace GameEngine {
ParticleMaterial::ParticleMaterial() {}

void ParticleMaterial::Create(GraphicsDevice* device, const Vector4& color) {
   // マテリアル用バッファを作成
   materialResource_ = ResourceHelper::CreateBufferResource(
	  device->GetDevice(),
	  sizeof(MaterialData)
   );

   // マッピング
   materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

   // 初期値設定
   materialData_->color = color;
   materialData_->uvTransform = MakeIdentity4x4();
}

void ParticleMaterial::SetColor(const Vector4& color) {
   if (materialData_) {
	  materialData_->color = color;
   }
}

void ParticleMaterial::SetUVTransform(const Matrix4x4& transform) {
   if (materialData_) {
	  materialData_->uvTransform = transform;
   }
}
}
