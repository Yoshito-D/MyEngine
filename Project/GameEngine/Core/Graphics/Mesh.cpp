#include "pch.h"
#include "Mesh.h"
#include "ResourceHelper.h"
#include "Core/Window/Window.h"
#include "Utility/MathUtils.h"
#include "GraphicsDevice.h"


namespace GameEngine {
namespace {
Logger& log_ = Logger::GetInstance();
GraphicsDevice* sDevice_ = nullptr;
bool sIsInitialized_ = false;
}

void Mesh::Initialize(GraphicsDevice* device) {
   if (sIsInitialized_) return;
   sDevice_ = device;
   sIsInitialized_ = true;
}

void Mesh::CreateSprite(float width, float height) {
   if (!sIsInitialized_)return;
   vertexResource_ = ResourceHelper::CreateBufferResource(sDevice_->GetDevice(), sizeof(VertexData) * 4);
   vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
   vertexBufferView_.SizeInBytes = sizeof(VertexData) * 4;
   vertexBufferView_.StrideInBytes = sizeof(VertexData);
   vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

   // 左下
   vertexData_[0].position = { 0, height, 0.0f, 1.0f };
   vertexData_[0].texCoord = { 0.0f, 1.0f };
   vertexData_[0].normal = { 0.0f, 0.0f, -1.0f };

   // 左上
   vertexData_[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };
   vertexData_[1].texCoord = { 0.0f, 0.0f };
   vertexData_[1].normal = { 0.0f, 0.0f, -1.0f };

   // 右下
   vertexData_[2].position = { width, height, 0.0f, 1.0f };
   vertexData_[2].texCoord = { 1.0f, 1.0f };
   vertexData_[2].normal = { 0.0f, 0.0f, -1.0f };

   // 右上
   vertexData_[3].position = { width, 0.0f,0.0f, 1.0f };
   vertexData_[3].texCoord = { 1.0f, 0.0f };
   vertexData_[3].normal = { 0.0f, 0.0f, -1.0f };

   indexResource_ = ResourceHelper::CreateBufferResource(sDevice_->GetDevice(), sizeof(uint32_t) * 6);

   indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();

   indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;

   indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

   uint32_t* indexData_ = nullptr;
   indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
   indexData_[0] = 0;
   indexData_[1] = 1;
   indexData_[2] = 2;
   indexData_[3] = 1;
   indexData_[4] = 3;
   indexData_[5] = 2;

   indexCount_ = 6;
}

void Mesh::CreateParticleQuad(float width, float height) {
   if (!sIsInitialized_)return;

   vertexResource_ = ResourceHelper::CreateBufferResource(sDevice_->GetDevice(), sizeof(VertexData) * 4);
   vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
   vertexBufferView_.SizeInBytes = sizeof(VertexData) * 4;
   vertexBufferView_.StrideInBytes = sizeof(VertexData);
   vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

   // パーティクル用: 中心を原点とし、テクスチャ座標を反転
   float halfWidth = width * 0.5f;
   float halfHeight = height * 0.5f;

   // 左下
   vertexData_[0].position = { -halfWidth, -halfHeight, 0.0f, 1.0f };
   vertexData_[0].texCoord = { 0.0f, 1.0f };
   vertexData_[0].normal = { 0.0f, 0.0f, -1.0f };

   // 左上
   vertexData_[1].position = { -halfWidth, halfHeight, 0.0f, 1.0f };
   vertexData_[1].texCoord = { 0.0f, 0.0f };
   vertexData_[1].normal = { 0.0f, 0.0f, -1.0f };

   // 右下
   vertexData_[2].position = { halfWidth, -halfHeight, 0.0f, 1.0f };
   vertexData_[2].texCoord = { 1.0f, 1.0f };
   vertexData_[2].normal = { 0.0f, 0.0f, -1.0f };

   // 右上
   vertexData_[3].position = { halfWidth, halfHeight, 0.0f, 1.0f };
   vertexData_[3].texCoord = { 1.0f, 0.0f };
   vertexData_[3].normal = { 0.0f, 0.0f, -1.0f };

   indexResource_ = ResourceHelper::CreateBufferResource(sDevice_->GetDevice(), sizeof(uint32_t) * 6);
   indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
   indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
   indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

   uint32_t* indexData_ = nullptr;
   indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
   indexData_[0] = 0;
   indexData_[1] = 1;
   indexData_[2] = 2;
   indexData_[3] = 1;
   indexData_[4] = 3;
   indexData_[5] = 2;

   indexCount_ = 6;
}

Mesh::VertexData* Mesh::GetVertexData() const {
   return vertexData_;
}
}
