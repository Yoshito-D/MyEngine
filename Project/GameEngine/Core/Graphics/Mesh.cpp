#include "pch.h"
#include "Mesh.h"
#include "ResourceHelper.h"
#include "Core/Window/Window.h"
#include "Utility/MathUtils.h"
#include "GraphicsDevice.h"
#include <algorithm>
#include <numbers>


namespace GameEngine {
namespace {
Logger& log_ = Logger::GetInstance();
GraphicsDevice* sDevice_ = nullptr;
bool sIsInitialized_ = false;

// 頂点バッファとインデックスバッファを確保し、ポインタを返すヘルパー
void AllocateMesh(
   uint32_t vertexCount, uint32_t indexCount,
   ComPtr<ID3D12Resource>& vertexResource, D3D12_VERTEX_BUFFER_VIEW& vertexView, Mesh::VertexData*& vertexData,
   ComPtr<ID3D12Resource>& indexResource, D3D12_INDEX_BUFFER_VIEW& indexView, uint32_t*& indexData)
{
   vertexResource = ResourceHelper::CreateBufferResource(sDevice_->GetDevice(), sizeof(Mesh::VertexData) * vertexCount);
   vertexView.BufferLocation = vertexResource->GetGPUVirtualAddress();
   vertexView.SizeInBytes = sizeof(Mesh::VertexData) * vertexCount;
   vertexView.StrideInBytes = sizeof(Mesh::VertexData);
   vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

   indexResource = ResourceHelper::CreateBufferResource(sDevice_->GetDevice(), sizeof(uint32_t) * indexCount);
   indexView.BufferLocation = indexResource->GetGPUVirtualAddress();
   indexView.SizeInBytes = sizeof(uint32_t) * indexCount;
   indexView.Format = DXGI_FORMAT_R32_UINT;
   indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
}

Vector3 TransformPlanePoint(float u, float v, Mesh::PlaneOrientation orientation) {
   switch (orientation) {
   case Mesh::PlaneOrientation::XZ: return { u, 0.0f, v };
   case Mesh::PlaneOrientation::YZ: return { 0.0f, u, v };
   case Mesh::PlaneOrientation::XY:
   default:                         return { u, v, 0.0f };
   }
}

Vector3 PlaneNormal(Mesh::PlaneOrientation orientation) {
   switch (orientation) {
   case Mesh::PlaneOrientation::XZ: return { 0.0f, 1.0f, 0.0f };
   case Mesh::PlaneOrientation::YZ: return { 1.0f, 0.0f, 0.0f };
   case Mesh::PlaneOrientation::XY:
   default:                         return { 0.0f, 0.0f, -1.0f };
   }
}

float VerticalOriginOffset(float height, float originY) {
   return height * (0.5f - std::clamp(originY, 0.0f, 1.0f));
}
}

void Mesh::Initialize(GraphicsDevice* device) {
   if (sIsInitialized_) return;
   sDevice_ = device;
   sIsInitialized_ = true;
}

void Mesh::CreateSprite(float width, float height, PlaneOrientation orientation) {
   if (!sIsInitialized_)return;
   vertexResource_ = ResourceHelper::CreateBufferResource(sDevice_->GetDevice(), sizeof(VertexData) * 4);
   vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
   vertexBufferView_.SizeInBytes = sizeof(VertexData) * 4;
   vertexBufferView_.StrideInBytes = sizeof(VertexData);
   vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

   Vector3 n = PlaneNormal(orientation);

   // 左下
   {
      Vector3 p = TransformPlanePoint(0.0f, height, orientation);
      vertexData_[0].position = { p.x, p.y, p.z, 1.0f };
      vertexData_[0].texCoord = { 0.0f, 1.0f };
      vertexData_[0].normal = n;
   }

   // 左上
   {
      Vector3 p = TransformPlanePoint(0.0f, 0.0f, orientation);
      vertexData_[1].position = { p.x, p.y, p.z, 1.0f };
      vertexData_[1].texCoord = { 0.0f, 0.0f };
      vertexData_[1].normal = n;
   }

   // 右下
   {
      Vector3 p = TransformPlanePoint(width, height, orientation);
      vertexData_[2].position = { p.x, p.y, p.z, 1.0f };
      vertexData_[2].texCoord = { 1.0f, 1.0f };
      vertexData_[2].normal = n;
   }

   // 右上
   {
      Vector3 p = TransformPlanePoint(width, 0.0f, orientation);
      vertexData_[3].position = { p.x, p.y, p.z, 1.0f };
      vertexData_[3].texCoord = { 1.0f, 0.0f };
      vertexData_[3].normal = n;
   }

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

void Mesh::CreateParticleQuad(float width, float height, PlaneOrientation orientation) {
   if (!sIsInitialized_)return;

   vertexResource_ = ResourceHelper::CreateBufferResource(sDevice_->GetDevice(), sizeof(VertexData) * 4);
   vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
   vertexBufferView_.SizeInBytes = sizeof(VertexData) * 4;
   vertexBufferView_.StrideInBytes = sizeof(VertexData);
   vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

   // パーティクル用: 中心を原点とし、テクスチャ座標を反転
   float halfWidth = width * 0.5f;
   float halfHeight = height * 0.5f;

   Vector3 n = PlaneNormal(orientation);

   // 左下
   {
      Vector3 p = TransformPlanePoint(-halfWidth, -halfHeight, orientation);
      vertexData_[0].position = { p.x, p.y, p.z, 1.0f };
      vertexData_[0].texCoord = { 0.0f, 1.0f };
      vertexData_[0].normal = n;
   }

   // 左上
   {
      Vector3 p = TransformPlanePoint(-halfWidth, halfHeight, orientation);
      vertexData_[1].position = { p.x, p.y, p.z, 1.0f };
      vertexData_[1].texCoord = { 0.0f, 0.0f };
      vertexData_[1].normal = n;
   }

   // 右下
   {
      Vector3 p = TransformPlanePoint(halfWidth, -halfHeight, orientation);
      vertexData_[2].position = { p.x, p.y, p.z, 1.0f };
      vertexData_[2].texCoord = { 1.0f, 1.0f };
      vertexData_[2].normal = n;
   }

   // 右上
   {
      Vector3 p = TransformPlanePoint(halfWidth, halfHeight, orientation);
      vertexData_[3].position = { p.x, p.y, p.z, 1.0f };
      vertexData_[3].texCoord = { 1.0f, 0.0f };
      vertexData_[3].normal = n;
   }

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

void Mesh::CreateSkybox() {
   if (!sIsInitialized_) return;

   // 各面: { 4頂点の { position, texCoord, normal } }, 法線は内向き
   struct FaceVertex { Vector4 pos; Vector2 uv; Vector3 norm; };
   static constexpr FaceVertex kFaces[6][4] = {
	  // +X (右)
	  { { { 1.0f,-1.0f, 1.0f,1.0f},{0.0f,1.0f},{-1.0f,0.0f, 0.0f} },
		{ { 1.0f, 1.0f, 1.0f,1.0f},{0.0f,0.0f},{-1.0f,0.0f, 0.0f} },
		{ { 1.0f,-1.0f,-1.0f,1.0f},{1.0f,1.0f},{-1.0f,0.0f, 0.0f} },
		{ { 1.0f, 1.0f,-1.0f,1.0f},{1.0f,0.0f},{-1.0f,0.0f, 0.0f} }
	  },
	  // -X (左)
	  { { {-1.0f,-1.0f,-1.0f,1.0f},{0.0f,1.0f},{ 1.0f,0.0f, 0.0f} },
		{ {-1.0f, 1.0f,-1.0f,1.0f},{0.0f,0.0f},{ 1.0f,0.0f, 0.0f} },
		{ {-1.0f,-1.0f, 1.0f,1.0f},{1.0f,1.0f},{ 1.0f,0.0f, 0.0f} },
		{ {-1.0f, 1.0f, 1.0f,1.0f},{1.0f,0.0f},{ 1.0f,0.0f, 0.0f} }
	  },
	  // +Y (上)
	  { { {-1.0f, 1.0f, 1.0f,1.0f},{0.0f,1.0f},{ 0.0f,-1.0f,0.0f} },
		{ {-1.0f, 1.0f,-1.0f,1.0f},{0.0f,0.0f},{ 0.0f,-1.0f,0.0f} },
		{ { 1.0f, 1.0f, 1.0f,1.0f},{1.0f,1.0f},{ 0.0f,-1.0f,0.0f} },
		{ { 1.0f, 1.0f,-1.0f,1.0f},{1.0f,0.0f},{ 0.0f,-1.0f,0.0f} }
	  },
	  // -Y (下)
	  { { {-1.0f,-1.0f,-1.0f,1.0f},{0.0f,1.0f},{ 0.0f, 1.0f,0.0f} },
		{ {-1.0f,-1.0f, 1.0f,1.0f},{0.0f,0.0f},{ 0.0f, 1.0f,0.0f} },
		{ { 1.0f,-1.0f,-1.0f,1.0f},{1.0f,1.0f},{ 0.0f, 1.0f,0.0f} },
		{ { 1.0f,-1.0f, 1.0f,1.0f},{1.0f,0.0f},{ 0.0f, 1.0f,0.0f} }
	  },
	  // +Z (前)
	  { { {-1.0f,-1.0f, 1.0f,1.0f},{0.0f,1.0f},{ 0.0f,0.0f,-1.0f} },
		{ {-1.0f, 1.0f, 1.0f,1.0f},{0.0f,0.0f},{ 0.0f,0.0f,-1.0f} },
		{ { 1.0f,-1.0f, 1.0f,1.0f},{1.0f,1.0f},{ 0.0f,0.0f,-1.0f} },
		{ { 1.0f, 1.0f, 1.0f,1.0f},{1.0f,0.0f},{ 0.0f,0.0f,-1.0f} }
	  },
	  // -Z (後)
	  { { { 1.0f,-1.0f,-1.0f,1.0f},{0.0f,1.0f},{ 0.0f,0.0f, 1.0f} },
		{ { 1.0f, 1.0f,-1.0f,1.0f},{0.0f,0.0f},{ 0.0f,0.0f, 1.0f} },
		{ {-1.0f,-1.0f,-1.0f,1.0f},{1.0f,1.0f},{ 0.0f,0.0f, 1.0f} },
		{ {-1.0f, 1.0f,-1.0f,1.0f},{1.0f,0.0f},{ 0.0f,0.0f, 1.0f} }
	  },
   };

   constexpr uint32_t kVertexCount = 24;
   constexpr uint32_t kIndexCount = 36;

   uint32_t* indexData = nullptr;
   AllocateMesh(kVertexCount, kIndexCount,
	  vertexResource_, vertexBufferView_, vertexData_,
	  indexResource_, indexBufferView_, indexData);

   for (uint32_t face = 0; face < 6; ++face) {
	  for (uint32_t v = 0; v < 4; ++v) {
		 uint32_t idx = face * 4 + v;
		 vertexData_[idx].position = kFaces[face][v].pos;
		 vertexData_[idx].texCoord = kFaces[face][v].uv;
		 vertexData_[idx].normal = kFaces[face][v].norm;
	  }
	  uint32_t base = face * 4;
	  uint32_t i = face * 6;
	  indexData[i + 0] = base + 0; indexData[i + 1] = base + 1; indexData[i + 2] = base + 2;
	  indexData[i + 3] = base + 1; indexData[i + 4] = base + 3; indexData[i + 5] = base + 2;
   }

   indexCount_ = kIndexCount;
}

void Mesh::CreateRing(float innerRadius, float outerRadius, uint32_t segmentCount, PlaneOrientation orientation) {
   if (!sIsInitialized_) return;

   const uint32_t kVertexCount = segmentCount * 2;
   const uint32_t kIndexCount = segmentCount * 6;
   const float    kStep = 2.0f * std::numbers::pi_v<float> / static_cast<float>(segmentCount);

   uint32_t* indexData = nullptr;
   AllocateMesh(kVertexCount, kIndexCount,
	  vertexResource_, vertexBufferView_, vertexData_,
	  indexResource_, indexBufferView_, indexData);

   Vector3 n = PlaneNormal(orientation);

   for (uint32_t i = 0; i < segmentCount; ++i) {
	  float theta = kStep * static_cast<float>(i);
	  float cos = std::cosf(theta);
	  float sin = std::sinf(theta);
	  float u = static_cast<float>(i) / static_cast<float>(segmentCount);

	  // 外側頂点
	  {
		 Vector3 p = TransformPlanePoint(outerRadius * cos, outerRadius * sin, orientation);
		 vertexData_[i * 2 + 0].position = { p.x, p.y, p.z, 1.0f };
	  }
	  vertexData_[i * 2 + 0].texCoord = { u, 0.0f };
	  vertexData_[i * 2 + 0].normal = n;
	  // 内側頂点
	  {
		 Vector3 p = TransformPlanePoint(innerRadius * cos, innerRadius * sin, orientation);
		 vertexData_[i * 2 + 1].position = { p.x, p.y, p.z, 1.0f };
	  }
	  vertexData_[i * 2 + 1].texCoord = { u, 1.0f };
	  vertexData_[i * 2 + 1].normal = n;
   }

   for (uint32_t i = 0; i < segmentCount; ++i) {
	  uint32_t next = (i + 1) % segmentCount;
	  uint32_t base = i * 6;
	  indexData[base + 0] = i * 2 + 0;
	  indexData[base + 1] = next * 2 + 0;
	  indexData[base + 2] = i * 2 + 1;
	  indexData[base + 3] = next * 2 + 0;
	  indexData[base + 4] = next * 2 + 1;
	  indexData[base + 5] = i * 2 + 1;
   }

   indexCount_ = kIndexCount;
}

void Mesh::CreatePlane(float width, float depth, uint32_t widthSegments, uint32_t depthSegments, PlaneOrientation orientation) {
   if (!sIsInitialized_) return;

   const uint32_t kCols = widthSegments + 1;
   const uint32_t kRows = depthSegments + 1;
   const uint32_t kVertexCount = kCols * kRows;
   const uint32_t kIndexCount = widthSegments * depthSegments * 6;

   uint32_t* indexData = nullptr;
   AllocateMesh(kVertexCount, kIndexCount,
	  vertexResource_, vertexBufferView_, vertexData_,
	  indexResource_, indexBufferView_, indexData);

   Vector3 n = PlaneNormal(orientation);

   for (uint32_t row = 0; row < kRows; ++row) {
	  for (uint32_t col = 0; col < kCols; ++col) {
		 float u = static_cast<float>(col) / static_cast<float>(widthSegments);
		 float v = static_cast<float>(row) / static_cast<float>(depthSegments);
		 uint32_t idx = row * kCols + col;
		 Vector3 p = TransformPlanePoint((u - 0.5f) * width, (v - 0.5f) * depth, orientation);
		 vertexData_[idx].position = { p.x, p.y, p.z, 1.0f };
		 vertexData_[idx].texCoord = { u, v };
		 vertexData_[idx].normal = n;
	  }
   }

   uint32_t idx = 0;
   for (uint32_t row = 0; row < depthSegments; ++row) {
	  for (uint32_t col = 0; col < widthSegments; ++col) {
		 uint32_t a = row * kCols + col;
		 uint32_t b = a + kCols;
		 indexData[idx++] = a;     indexData[idx++] = b;     indexData[idx++] = a + 1;
		 indexData[idx++] = b;     indexData[idx++] = b + 1; indexData[idx++] = a + 1;
	  }
   }

   indexCount_ = kIndexCount;
}

void Mesh::CreateCircle(float radius, uint32_t segmentCount, PlaneOrientation orientation) {
   if (!sIsInitialized_) return;

   const uint32_t kVertexCount = segmentCount + 1; // 中心 + 外周
   const uint32_t kIndexCount = segmentCount * 3;
   const float    kStep = 2.0f * std::numbers::pi_v<float> / static_cast<float>(segmentCount);

   uint32_t* indexData = nullptr;
   AllocateMesh(kVertexCount, kIndexCount,
	  vertexResource_, vertexBufferView_, vertexData_,
	  indexResource_, indexBufferView_, indexData);

   Vector3 n = PlaneNormal(orientation);

   // 中心
   vertexData_[0].position = { 0.0f, 0.0f, 0.0f, 1.0f };
   vertexData_[0].texCoord = { 0.5f, 0.5f };
   vertexData_[0].normal = n;

   for (uint32_t i = 0; i < segmentCount; ++i) {
	  float theta = kStep * static_cast<float>(i);
	  float cos = std::cosf(theta);
	  float sin = std::sinf(theta);
	  Vector3 p = TransformPlanePoint(radius * cos, radius * sin, orientation);
	  vertexData_[i + 1].position = { p.x, p.y, p.z, 1.0f };
	  vertexData_[i + 1].texCoord = { cos * 0.5f + 0.5f, sin * 0.5f + 0.5f };
	  vertexData_[i + 1].normal = n;

	  indexData[i * 3 + 0] = 0;
	  indexData[i * 3 + 1] = i + 1;
	  indexData[i * 3 + 2] = (i + 1) % segmentCount + 1;
   }

   indexCount_ = kIndexCount;
}

void Mesh::CreateBox(float width, float height, float depth, float originY) {
   if (!sIsInitialized_) return;

   // 各面: { 4頂点 } × 6面
   struct FaceVertex { Vector4 pos; Vector2 uv; Vector3 norm; };
   const float hw = width * 0.5f;
   const float hh = height * 0.5f;
   const float hd = depth * 0.5f;
   const float yOffset = VerticalOriginOffset(height, originY);

   const FaceVertex kFaces[6][4] = {
	  // +X
	  { {{hw,-hh, hd,1},{0,1},{ 1,0, 0}}, {{hw, hh, hd,1},{0,0},{ 1,0, 0}},
		{{hw,-hh,-hd,1},{1,1},{ 1,0, 0}}, {{hw, hh,-hd,1},{1,0},{ 1,0, 0}}
	  },
	  // -X
	  { {{-hw,-hh,-hd,1},{0,1},{-1,0, 0}}, {{-hw, hh,-hd,1},{0,0},{-1,0, 0}},
		{{-hw,-hh, hd,1},{1,1},{-1,0, 0}}, {{-hw, hh, hd,1},{1,0},{-1,0, 0}}
	  },
	  // +Y
	  { {{-hw, hh, hd,1},{0,1},{ 0,1, 0}}, {{-hw, hh,-hd,1},{0,0},{ 0,1, 0}},
	    {{ hw, hh, hd,1},{1,1},{ 0,1, 0}}, {{ hw, hh,-hd,1},{1,0},{ 0,1, 0}}
	  },
	  // -Y
	  { {{-hw,-hh,-hd,1},{0,1},{ 0,-1,0}}, {{-hw,-hh, hd,1},{0,0},{ 0,-1,0}},
		{{ hw,-hh,-hd,1},{1,1},{ 0,-1,0}}, {{ hw,-hh, hd,1},{1,0},{ 0,-1,0}}
	  },
	  // +Z
	  { {{-hw,-hh, hd,1},{0,1},{ 0,0, 1}}, {{-hw, hh, hd,1},{0,0},{ 0,0, 1}},
	    {{ hw,-hh, hd,1},{1,1},{ 0,0, 1}}, {{ hw, hh, hd,1},{1,0},{ 0,0, 1}}
	  },
	  // -Z
	  { {{ hw,-hh,-hd,1},{0,1},{ 0,0,-1}}, {{ hw, hh,-hd,1},{0,0},{ 0,0,-1}},
		{{-hw,-hh,-hd,1},{1,1},{ 0,0,-1}}, {{-hw, hh,-hd,1},{1,0},{ 0,0,-1}}
	  },
   };

   constexpr uint32_t kVertexCount = 24;
   constexpr uint32_t kIndexCount = 36;

   uint32_t* indexData = nullptr;
   AllocateMesh(kVertexCount, kIndexCount,
	  vertexResource_, vertexBufferView_, vertexData_,
	  indexResource_, indexBufferView_, indexData);

   for (uint32_t face = 0; face < 6; ++face) {
	  for (uint32_t v = 0; v < 4; ++v) {
		 uint32_t idx = face * 4 + v;
		 Vector4 position = kFaces[face][v].pos;
		 position.y += yOffset;
		 vertexData_[idx].position = position;
		 vertexData_[idx].texCoord = kFaces[face][v].uv;
		 vertexData_[idx].normal = kFaces[face][v].norm;
	  }
	  uint32_t base = face * 4, i = face * 6;
	  indexData[i + 0] = base + 0; indexData[i + 1] = base + 1; indexData[i + 2] = base + 2;
	  indexData[i + 3] = base + 1; indexData[i + 4] = base + 3; indexData[i + 5] = base + 2;
   }

   indexCount_ = kIndexCount;
}

void Mesh::CreateSphere(float radius, uint32_t stackCount, uint32_t sliceCount, float originY) {
   if (!sIsInitialized_) return;

   const uint32_t kVertexCount = (stackCount + 1) * (sliceCount + 1);
   const uint32_t kIndexCount = stackCount * sliceCount * 6;
   const float yOffset = VerticalOriginOffset(radius * 2.0f, originY);

   uint32_t* indexData = nullptr;
   AllocateMesh(kVertexCount, kIndexCount,
	  vertexResource_, vertexBufferView_, vertexData_,
	  indexResource_, indexBufferView_, indexData);

   for (uint32_t stack = 0; stack <= stackCount; ++stack) {
	  float phi = std::numbers::pi_v<float> *static_cast<float>(stack) / static_cast<float>(stackCount);
	  float v = static_cast<float>(stack) / static_cast<float>(stackCount);
	  for (uint32_t slice = 0; slice <= sliceCount; ++slice) {
		 float theta = 2.0f * std::numbers::pi_v<float> *static_cast<float>(slice) / static_cast<float>(sliceCount);
		 float u = static_cast<float>(slice) / static_cast<float>(sliceCount);
		 float x = std::sinf(phi) * std::cosf(theta);
		 float y = std::cosf(phi);
		 float z = std::sinf(phi) * std::sinf(theta);
		 uint32_t idx = stack * (sliceCount + 1) + slice;
		 vertexData_[idx].position = { x * radius, y * radius + yOffset, z * radius, 1.0f };
		 vertexData_[idx].texCoord = { u, v };
		 vertexData_[idx].normal = { x, y, z };
	  }
   }

   uint32_t idx = 0;
   for (uint32_t stack = 0; stack < stackCount; ++stack) {
	  for (uint32_t slice = 0; slice < sliceCount; ++slice) {
		 uint32_t a = stack * (sliceCount + 1) + slice;
		 uint32_t b = (stack + 1) * (sliceCount + 1) + slice;
		 indexData[idx++] = a;   indexData[idx++] = b;   indexData[idx++] = a + 1;
		 indexData[idx++] = b;   indexData[idx++] = b + 1; indexData[idx++] = a + 1;
	  }
   }

   indexCount_ = kIndexCount;
}

void Mesh::CreateTorus(float majorRadius, float minorRadius, uint32_t majorSegments, uint32_t minorSegments, float originY) {
   if (!sIsInitialized_) return;

   const uint32_t kVertexCount = (majorSegments + 1) * (minorSegments + 1);
   const uint32_t kIndexCount = majorSegments * minorSegments * 6;
   const float yOffset = VerticalOriginOffset(minorRadius * 2.0f, originY);

   uint32_t* indexData = nullptr;
   AllocateMesh(kVertexCount, kIndexCount,
	  vertexResource_, vertexBufferView_, vertexData_,
	  indexResource_, indexBufferView_, indexData);

   for (uint32_t i = 0; i <= majorSegments; ++i) {
	  float phi = 2.0f * std::numbers::pi_v<float> *static_cast<float>(i) / static_cast<float>(majorSegments);
	  float cosPhi = std::cosf(phi);
	  float sinPhi = std::sinf(phi);
	  for (uint32_t j = 0; j <= minorSegments; ++j) {
		 float theta = 2.0f * std::numbers::pi_v<float> *static_cast<float>(j) / static_cast<float>(minorSegments);
		 float cosTheta = std::cosf(theta);
		 float sinTheta = std::sinf(theta);
		 float cx = majorRadius * cosPhi;
		 float cz = majorRadius * sinPhi;
		 float x = (majorRadius + minorRadius * cosTheta) * cosPhi;
		 float localY = minorRadius * sinTheta;
		 float y = localY + yOffset;
		 float z = (majorRadius + minorRadius * cosTheta) * sinPhi;
		 float nx = (x - cx) / minorRadius;
		 float nz = (z - cz) / minorRadius;
		 uint32_t idx = i * (minorSegments + 1) + j;
		 vertexData_[idx].position = { x, y, z, 1.0f };
		 vertexData_[idx].texCoord = { static_cast<float>(i) / majorSegments, static_cast<float>(j) / minorSegments };
		 vertexData_[idx].normal = { nx, localY / minorRadius, nz };
	  }
   }

   uint32_t idx = 0;
   for (uint32_t i = 0; i < majorSegments; ++i) {
	  for (uint32_t j = 0; j < minorSegments; ++j) {
		 uint32_t a = i * (minorSegments + 1) + j;
		 uint32_t b = (i + 1) * (minorSegments + 1) + j;
		 indexData[idx++] = a;   indexData[idx++] = b;   indexData[idx++] = a + 1;
		 indexData[idx++] = b;   indexData[idx++] = b + 1; indexData[idx++] = a + 1;
	  }
   }

   indexCount_ = kIndexCount;
}

void Mesh::CreateCylinder(float topRadius, float bottomRadius, float height, uint32_t segmentCount, float originY) {
   if (!sIsInitialized_) return;

   // 側面頂点: (segmentCount+1) × 2 + 中心頂点 2 個 + 上下外周各 segmentCount 個
   // 構造: [側面頂点] [上蓋中心] [上蓋外周] [下蓋中心] [下蓋外周]
   const uint32_t kSideVerts = (segmentCount + 1) * 2;
   const uint32_t kCapVerts = segmentCount + 1; // 中心1 + 外周 segmentCount
   const uint32_t kVertexCount = kSideVerts + kCapVerts * 2;
   const uint32_t kSideIdx = segmentCount * 6;
   const uint32_t kCapIdx = segmentCount * 3;
   const uint32_t kIndexCount = kSideIdx + kCapIdx * 2;
   const float    hh = height * 0.5f;
   const float    yOffset = VerticalOriginOffset(height, originY);
   const float    sideSlope = height != 0.0f ? (topRadius - bottomRadius) / height : 0.0f;
   const float    normalY = -sideSlope;
   const float    normalScale = 1.0f / std::sqrtf(1.0f + normalY * normalY);
   const float    kStep = 2.0f * std::numbers::pi_v<float> / static_cast<float>(segmentCount);

   uint32_t* indexData = nullptr;
   AllocateMesh(kVertexCount, kIndexCount,
	  vertexResource_, vertexBufferView_, vertexData_,
	  indexResource_, indexBufferView_, indexData);

   // 側面
   for (uint32_t i = 0; i <= segmentCount; ++i) {
	  float theta = kStep * static_cast<float>(i);
	  float cos = std::cosf(theta);
	  float sin = std::sinf(theta);
	  float u = static_cast<float>(i) / static_cast<float>(segmentCount);
	  vertexData_[i * 2 + 0].position = { topRadius * cos,  hh + yOffset, topRadius * sin, 1.0f };
	  vertexData_[i * 2 + 0].texCoord = { u, 0.0f };
	  vertexData_[i * 2 + 0].normal = { cos * normalScale, normalY * normalScale, sin * normalScale };
	  vertexData_[i * 2 + 1].position = { bottomRadius * cos, -hh + yOffset, bottomRadius * sin, 1.0f };
	  vertexData_[i * 2 + 1].texCoord = { u, 1.0f };
	  vertexData_[i * 2 + 1].normal = { cos * normalScale, normalY * normalScale, sin * normalScale };
   }

   // 側面インデックス
   for (uint32_t i = 0; i < segmentCount; ++i) {
	  uint32_t base = i * 6;
	  indexData[base + 0] = i * 2 + 0; indexData[base + 1] = i * 2 + 2; indexData[base + 2] = i * 2 + 1;
	  indexData[base + 3] = i * 2 + 2; indexData[base + 4] = i * 2 + 3; indexData[base + 5] = i * 2 + 1;
   }

   // 上蓋 (Y=+hh, 法線 Y+)
   uint32_t topBase = kSideVerts;
   vertexData_[topBase].position = { 0.0f, hh + yOffset, 0.0f, 1.0f };
   vertexData_[topBase].texCoord = { 0.5f, 0.5f };
   vertexData_[topBase].normal = { 0.0f, 1.0f, 0.0f };
   for (uint32_t i = 0; i < segmentCount; ++i) {
	  float theta = kStep * static_cast<float>(i);
	  float cos = std::cosf(theta);
	  float sin = std::sinf(theta);
	  vertexData_[topBase + 1 + i].position = { topRadius * cos, hh + yOffset, topRadius * sin, 1.0f };
	  vertexData_[topBase + 1 + i].texCoord = { cos * 0.5f + 0.5f, sin * 0.5f + 0.5f };
	  vertexData_[topBase + 1 + i].normal = { 0.0f, 1.0f, 0.0f };
	  indexData[kSideIdx + i * 3 + 0] = topBase;
	  indexData[kSideIdx + i * 3 + 1] = topBase + 1 + (i + 1) % segmentCount;
	  indexData[kSideIdx + i * 3 + 2] = topBase + 1 + i;
   }

   // 下蓋 (Y=-hh, 法線 Y-)
   uint32_t botBase = kSideVerts + kCapVerts;
   vertexData_[botBase].position = { 0.0f, -hh + yOffset, 0.0f, 1.0f };
   vertexData_[botBase].texCoord = { 0.5f, 0.5f };
   vertexData_[botBase].normal = { 0.0f, -1.0f, 0.0f };
   for (uint32_t i = 0; i < segmentCount; ++i) {
	  float theta = kStep * static_cast<float>(i);
	  float cos = std::cosf(theta);
	  float sin = std::sinf(theta);
	  vertexData_[botBase + 1 + i].position = { bottomRadius * cos, -hh + yOffset, bottomRadius * sin, 1.0f };
	  vertexData_[botBase + 1 + i].texCoord = { cos * 0.5f + 0.5f, sin * 0.5f + 0.5f };
	  vertexData_[botBase + 1 + i].normal = { 0.0f, -1.0f, 0.0f };
	  indexData[kSideIdx + kCapIdx + i * 3 + 0] = botBase;
	  indexData[kSideIdx + kCapIdx + i * 3 + 1] = botBase + 1 + i;
	  indexData[kSideIdx + kCapIdx + i * 3 + 2] = botBase + 1 + (i + 1) % segmentCount;
   }

   indexCount_ = kIndexCount;
}

void Mesh::CreateCylinderWithoutCaps(float topRadius, float bottomRadius, float height, uint32_t segmentCount, float originY) {
   if (!sIsInitialized_) return;

   const uint32_t kVertexCount = (segmentCount + 1) * 2;
   const uint32_t kIndexCount = segmentCount * 6;
   const float    hh = height * 0.5f;
   const float    yOffset = VerticalOriginOffset(height, originY);
   const float    sideSlope = height != 0.0f ? (topRadius - bottomRadius) / height : 0.0f;
   const float    normalY = -sideSlope;
   const float    normalScale = 1.0f / std::sqrtf(1.0f + normalY * normalY);
   const float    kStep = 2.0f * std::numbers::pi_v<float> / static_cast<float>(segmentCount);

   uint32_t* indexData = nullptr;
   AllocateMesh(kVertexCount, kIndexCount,
	  vertexResource_, vertexBufferView_, vertexData_,
	  indexResource_, indexBufferView_, indexData);

   for (uint32_t i = 0; i <= segmentCount; ++i) {
	  float theta = kStep * static_cast<float>(i);
	  float cos = std::cosf(theta);
	  float sin = std::sinf(theta);
	  float u = static_cast<float>(i) / static_cast<float>(segmentCount);
	  vertexData_[i * 2 + 0].position = { topRadius * cos,  hh + yOffset, topRadius * sin, 1.0f };
	  vertexData_[i * 2 + 0].texCoord = { u, 0.0f };
	  vertexData_[i * 2 + 0].normal = { cos * normalScale, normalY * normalScale, sin * normalScale };
	  vertexData_[i * 2 + 1].position = { bottomRadius * cos, -hh + yOffset, bottomRadius * sin, 1.0f };
	  vertexData_[i * 2 + 1].texCoord = { u, 1.0f };
	  vertexData_[i * 2 + 1].normal = { cos * normalScale, normalY * normalScale, sin * normalScale };
   }

   for (uint32_t i = 0; i < segmentCount; ++i) {
	  uint32_t base = i * 6;
	  indexData[base + 0] = i * 2 + 0; indexData[base + 1] = i * 2 + 2; indexData[base + 2] = i * 2 + 1;
	  indexData[base + 3] = i * 2 + 2; indexData[base + 4] = i * 2 + 3; indexData[base + 5] = i * 2 + 1;
   }

   indexCount_ = kIndexCount;
}

void Mesh::CreateCone(float radius, float height, uint32_t segmentCount, float originY) {
   if (!sIsInitialized_) return;

   // 側面: 頂点 1 + 底面外周 segmentCount
   // 底蓋: 中心 1 + 外周 segmentCount
   const uint32_t kSideVerts = 1 + segmentCount;
   const uint32_t kCapVerts = 1 + segmentCount;
   const uint32_t kVertexCount = kSideVerts + kCapVerts;
   const uint32_t kIndexCount = segmentCount * 3 * 2; // 側面 + 底面
   const float    hh = height * 0.5f;
   const float    yOffset = VerticalOriginOffset(height, originY);
   const float    kStep = 2.0f * std::numbers::pi_v<float> / static_cast<float>(segmentCount);
   const float    slopeLen = std::sqrtf(radius * radius + height * height);
   const float    ny = radius / slopeLen; // 側面法線の Y 成分

   uint32_t* indexData = nullptr;
   AllocateMesh(kVertexCount, kIndexCount,
	  vertexResource_, vertexBufferView_, vertexData_,
	  indexResource_, indexBufferView_, indexData);

   // 頂点 (apex)
   vertexData_[0].position = { 0.0f, hh + yOffset, 0.0f, 1.0f };
   vertexData_[0].texCoord = { 0.5f, 0.0f };
   vertexData_[0].normal = { 0.0f, 1.0f, 0.0f };

   for (uint32_t i = 0; i < segmentCount; ++i) {
	  float theta = kStep * static_cast<float>(i);
	  float cos = std::cosf(theta);
	  float sin = std::sinf(theta);
	  float nr = height / slopeLen;
	  vertexData_[1 + i].position = { radius * cos, -hh + yOffset, radius * sin, 1.0f };
	  vertexData_[1 + i].texCoord = { cos * 0.5f + 0.5f, 1.0f };
	  vertexData_[1 + i].normal = { cos * nr, ny, sin * nr };

	  indexData[i * 3 + 0] = 0;
	  indexData[i * 3 + 1] = 1 + i;
	  indexData[i * 3 + 2] = 1 + (i + 1) % segmentCount;
   }

   // 底蓋
   uint32_t botBase = kSideVerts;
   vertexData_[botBase].position = { 0.0f, -hh + yOffset, 0.0f, 1.0f };
   vertexData_[botBase].texCoord = { 0.5f, 0.5f };
   vertexData_[botBase].normal = { 0.0f, -1.0f, 0.0f };
   for (uint32_t i = 0; i < segmentCount; ++i) {
	  float theta = kStep * static_cast<float>(i);
	  float cos = std::cosf(theta);
	  float sin = std::sinf(theta);
	  vertexData_[botBase + 1 + i].position = { radius * cos, -hh + yOffset, radius * sin, 1.0f };
	  vertexData_[botBase + 1 + i].texCoord = { cos * 0.5f + 0.5f, sin * 0.5f + 0.5f };
	  vertexData_[botBase + 1 + i].normal = { 0.0f, -1.0f, 0.0f };

	  uint32_t base = segmentCount * 3 + i * 3;
	  indexData[base + 0] = botBase;
	  indexData[base + 1] = botBase + 1 + i;
	  indexData[base + 2] = botBase + 1 + (i + 1) % segmentCount;
   }

   indexCount_ = kIndexCount;
}

void Mesh::CreateTriangle(const Vector3& v0, const Vector3& v1, const Vector3& v2, PlaneOrientation orientation) {
   if (!sIsInitialized_) return;

   uint32_t* indexData = nullptr;
   AllocateMesh(3, 3,
	  vertexResource_, vertexBufferView_, vertexData_,
	  indexResource_, indexBufferView_, indexData);

   Vector3 p0 = TransformPlanePoint(v0.x, v0.y, orientation);
   Vector3 p1 = TransformPlanePoint(v1.x, v1.y, orientation);
   Vector3 p2 = TransformPlanePoint(v2.x, v2.y, orientation);

   // 法線: 外積で計算
   Vector3 edge1 = { p1.x - p0.x, p1.y - p0.y, p1.z - p0.z };
   Vector3 edge2 = { p2.x - p0.x, p2.y - p0.y, p2.z - p0.z };
   Vector3 norm = {
	  edge1.y * edge2.z - edge1.z * edge2.y,
	  edge1.z * edge2.x - edge1.x * edge2.z,
	  edge1.x * edge2.y - edge1.y * edge2.x
   };
   float len = std::sqrtf(norm.x * norm.x + norm.y * norm.y + norm.z * norm.z);
   if (len > 0.0f) { norm.x /= len; norm.y /= len; norm.z /= len; }

   vertexData_[0].position = { p0.x, p0.y, p0.z, 1.0f }; vertexData_[0].texCoord = { 0.0f, 1.0f }; vertexData_[0].normal = norm;
   vertexData_[1].position = { p1.x, p1.y, p1.z, 1.0f }; vertexData_[1].texCoord = { 0.5f, 0.0f }; vertexData_[1].normal = norm;
   vertexData_[2].position = { p2.x, p2.y, p2.z, 1.0f }; vertexData_[2].texCoord = { 1.0f, 1.0f }; vertexData_[2].normal = norm;

   indexData[0] = 0; indexData[1] = 1; indexData[2] = 2;

   indexCount_ = 3;
}

Mesh::VertexData* Mesh::GetVertexData() const {
   return vertexData_;
}
}
