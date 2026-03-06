#include "pch.h"
#include "LineRenderer.h"
#include "Scene/Camera/Camera.h"
#include "Utility/MathUtils.h"

namespace GameEngine {
void LineRenderer::Initialize(ID3D12Device* device, size_t maxLines) {
   maxLines_ = maxLines;

   // インスタンスバッファの作成
   size_t instanceBufferSize = sizeof(LineInstance) * maxLines;
   CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
   CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(instanceBufferSize);

   device->CreateCommittedResource(
	  &heapProps,
	  D3D12_HEAP_FLAG_NONE,
	  &resourceDesc,
	  D3D12_RESOURCE_STATE_GENERIC_READ,
	  nullptr,
	  IID_PPV_ARGS(&instanceBuffer_)
   );
   instanceBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedInstanceBuffer_));

   instanceVBView_.BufferLocation = instanceBuffer_->GetGPUVirtualAddress();
   instanceVBView_.StrideInBytes = sizeof(LineInstance);
   instanceVBView_.SizeInBytes = static_cast<UINT>(instanceBufferSize);

   // 基本頂点バッファの作成（2点のみ: インデックス0と1）
   struct BaseVertex {
	  float index; // 0 or 1
   };
   BaseVertex baseVertices[2] = { {0.0f}, {1.0f} };

   size_t baseVBSize = sizeof(BaseVertex) * 2;
   CD3DX12_RESOURCE_DESC baseResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(baseVBSize);

   device->CreateCommittedResource(
	  &heapProps,
	  D3D12_HEAP_FLAG_NONE,
	  &baseResourceDesc,
	  D3D12_RESOURCE_STATE_GENERIC_READ,
	  nullptr,
	  IID_PPV_ARGS(&baseVertexBuffer_)
   );

   UINT8* mappedBaseVB = nullptr;
   baseVertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedBaseVB));
   memcpy(mappedBaseVB, baseVertices, baseVBSize);
   baseVertexBuffer_->Unmap(0, nullptr);

   baseVBView_.BufferLocation = baseVertexBuffer_->GetGPUVirtualAddress();
   baseVBView_.StrideInBytes = sizeof(BaseVertex);
   baseVBView_.SizeInBytes = static_cast<UINT>(baseVBSize);

   transformationMatrix_.Create();
}

void LineRenderer::Begin() {
   cameraLineGroups_.clear();
}

void LineRenderer::AddLine(Camera* camera, const Line& line) {
   if (!camera) return;

   auto& lines = cameraLineGroups_[camera];
   if (lines.size() >= maxLines_) {
	  assert(false && "LineRenderer: max line count exceeded for this camera.");
	  return;
   }

   auto verts = line.GetVertices();
   LineInstance instance = {};
   instance.start = verts[0].position;
   instance.end = verts[1].position;
   instance.color = verts[0].color;
   lines.push_back(instance);
}

void LineRenderer::End() {
   // カメラごとにデータを整理（必要に応じて）
   // 現在はグループ化のみを行う
}

void LineRenderer::UpdateMatrix(const Matrix4x4& world, const Matrix4x4& viewProj) {
   auto* data = transformationMatrix_.GetTransformationMatrixData();
   data->world = world;
   data->wVP = world * viewProj;
}

void LineRenderer::Draw(ID3D12GraphicsCommandList* cmdList) {
   // このメソッドは描画関数内で呼び出され、
   // 既にインスタンスバッファにデータが書き込まれていることを前提とする

   cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

   // 頂点バッファとインスタンスバッファを設定
   D3D12_VERTEX_BUFFER_VIEW views[2] = { baseVBView_, instanceVBView_ };
   cmdList->IASetVertexBuffers(0, 2, views);

   cmdList->SetGraphicsRootConstantBufferView(0,
	  transformationMatrix_.GetTransformationMatrixResource()->GetGPUVirtualAddress());

   // インスタンシング描画は呼び出し元で制御される
   // （lineCountが渡される）
}

void LineRenderer::DrawLine(const Vector3& start, const Vector3& end, const Vector4& color, Camera* camera) {
   if (!camera) return;
   AddLine(camera, Line(start, end, color));
}

void LineRenderer::DrawSpline(const std::vector<Vector3>& controlPoints, const Vector4& color, size_t segmentCount, Camera* camera) {
   if (!camera || controlPoints.size() < 4 || segmentCount <= 0) return;

   std::vector<Vector3> pointsDrawing;
   pointsDrawing.clear();

   for (size_t i = 0; i < segmentCount + 1; i++) {
	  float t = 1.0f / segmentCount * i;
	  Vector3 pos = CatmullRomPosition(controlPoints, t);
	  pointsDrawing.push_back(pos);
   }

   for (size_t i = 0; i < pointsDrawing.size() - 1; i++) {
	  DrawLine(pointsDrawing[i], pointsDrawing[i + 1], color, camera);
   }
}

void LineRenderer::DrawGrid(Camera* camera, GridPlane plane, float gridSize, int thickLineInterval, int range, bool enableFade, float fadeDistance) {
   if (!camera) return;

   Vector3 cameraPos = camera->GetTransform().translation;

   // 基本グリッドの色設定
   const Vector4 baseGridColor(0.8f, 0.6f, 0.2f, 0.20f);
   const float minAlpha = 0.01f;
   const float baseAlpha = 0.20f;
   const float thickLineMinAlpha = 0.1f;
   const float thickLineAlpha = 0.8f;

   // 平面に応じた軸の色設定（初期値設定）
   Vector4 axis1Color = Vector4(1.0f, 0.0f, 0.0f, 0.8f);
   Vector4 axis2Color = Vector4(0.0f, 0.0f, 1.0f, 0.8f);
   Vector3 axis1Direction = Vector3(1.0f, 0.0f, 0.0f);
   Vector3 axis2Direction = Vector3(0.0f, 0.0f, 1.0f);
   float start1 = 0.0f, end1 = 0.0f, start2 = 0.0f, end2 = 0.0f;
   float cameraAxis1 = 0.0f, cameraAxis2 = 0.0f;

   switch (plane) {
	  case GridPlane::XZ:  // XZ平面（Y軸が上）
		 axis1Color = Vector4(1.0f, 0.3f, 0.2f, 0.8f);  // X軸（赤）
		 axis2Color = Vector4(0.2f, 0.6f, 1.0f, 0.8f);  // Z軸（青）
		 axis1Direction = Vector3(1.0f, 0.0f, 0.0f);
		 axis2Direction = Vector3(0.0f, 0.0f, 1.0f);
		 cameraAxis1 = cameraPos.x;
		 cameraAxis2 = cameraPos.z;
		 start1 = std::floor(cameraAxis1 / gridSize) * gridSize - range * gridSize;
		 end1 = start1 + range * 2 * gridSize;
		 start2 = std::floor(cameraAxis2 / gridSize) * gridSize - range * gridSize;
		 end2 = start2 + range * 2 * gridSize;
		 break;

	  case GridPlane::XY:  // XY平面（Z軸が上）
		 axis1Color = Vector4(1.0f, 0.3f, 0.2f, 0.8f);  // X軸（赤）
		 axis2Color = Vector4(0.2f, 1.0f, 0.3f, 0.8f);  // Y軸（緑）
		 axis1Direction = Vector3(1.0f, 0.0f, 0.0f);
		 axis2Direction = Vector3(0.0f, 1.0f, 0.0f);
		 cameraAxis1 = cameraPos.x;
		 cameraAxis2 = cameraPos.y;
		 start1 = std::floor(cameraAxis1 / gridSize) * gridSize - range * gridSize;
		 end1 = start1 + range * 2 * gridSize;
		 start2 = std::floor(cameraAxis2 / gridSize) * gridSize - range * gridSize;
		 end2 = start2 + range * 2 * gridSize;
		 break;

	  case GridPlane::YZ:  // YZ平面（X軸が上）
		 axis1Color = Vector4(0.2f, 1.0f, 0.3f, 0.8f);  // Y軸（緑）
		 axis2Color = Vector4(0.2f, 0.6f, 1.0f, 0.8f);  // Z軸（青）
		 axis1Direction = Vector3(0.0f, 1.0f, 0.0f);
		 axis2Direction = Vector3(0.0f, 0.0f, 1.0f);
		 cameraAxis1 = cameraPos.y;
		 cameraAxis2 = cameraPos.z;
		 start1 = std::floor(cameraAxis1 / gridSize) * gridSize - range * gridSize;
		 end1 = start1 + range * 2 * gridSize;
		 start2 = std::floor(cameraAxis2 / gridSize) * gridSize - range * gridSize;
		 end2 = start2 + range * 2 * gridSize;
		 break;
   }

   // 第1軸方向の線を描画
   for (float pos1 = start1; pos1 <= end1; pos1 += gridSize) {
	  Vector4 color;

	  float distance = std::abs(pos1 - cameraAxis1);
	  float fadeFactor = 1.0f;

	  // フェードが有効な場合のみ距離に応じたフェードを計算
	  if (enableFade) {
		 fadeFactor = std::clamp(1.0f - distance / fadeDistance, 0.0f, 1.0f);
	  }

	  // 原点の線は軸の色
	  if (std::abs(pos1) < 0.001f) {
		 color = axis2Color;
		 if (enableFade) {
			color.w *= fadeFactor;
		 }
	  }
	  // 指定間隔ごとに太い線
	  else if (thickLineInterval > 0 && std::fmod(std::abs(pos1), gridSize * thickLineInterval) < 0.001f) {
		 float alpha = enableFade
			? thickLineMinAlpha + (thickLineAlpha - thickLineMinAlpha) * fadeFactor
			: thickLineAlpha;
		 color = Vector4(baseGridColor.x, baseGridColor.y, baseGridColor.z, alpha);
	  }
	  // 通常の線
	  else {
		 float alpha = enableFade
			? minAlpha + (baseAlpha - minAlpha) * fadeFactor
			: baseAlpha;
		 color = Vector4(baseGridColor.x, baseGridColor.y, baseGridColor.z, alpha);
	  }

	  // 平面に応じて線を描画
	  Vector3 lineStart, lineEnd;
	  switch (plane) {
		 case GridPlane::XZ:
			lineStart = Vector3(pos1, 0.0f, start2);
			lineEnd = Vector3(pos1, 0.0f, end2);
			break;
		 case GridPlane::XY:
			lineStart = Vector3(pos1, start2, 0.0f);
			lineEnd = Vector3(pos1, end2, 0.0f);
			break;
		 case GridPlane::YZ:
			lineStart = Vector3(0.0f, pos1, start2);
			lineEnd = Vector3(0.0f, pos1, end2);
			break;
	  }

	  DrawLine(lineStart, lineEnd, color, camera);
   }

   // 第2軸方向の線を描画
   for (float pos2 = start2; pos2 <= end2; pos2 += gridSize) {
	  Vector4 color;

	  float distance = std::abs(pos2 - cameraAxis2);
	  float fadeFactor = 1.0f;

	  // フェードが有効な場合のみ距離に応じたフェードを計算
	  if (enableFade) {
		 fadeFactor = std::clamp(1.0f - distance / fadeDistance, 0.0f, 1.0f);
	  }

	  // 原点の線は軸の色
	  if (std::abs(pos2) < 0.001f) {
		 color = axis1Color;
		 if (enableFade) {
			color.w *= fadeFactor;
		 }
	  }
	  // 指定間隔ごとに太い線
	  else if (thickLineInterval > 0 && std::fmod(std::abs(pos2), gridSize * thickLineInterval) < 0.001f) {
		 float alpha = enableFade
			? thickLineMinAlpha + (thickLineAlpha - thickLineMinAlpha) * fadeFactor
			: thickLineAlpha;
		 color = Vector4(baseGridColor.x, baseGridColor.y, baseGridColor.z, alpha);
	  }
	  // 通常の線
	  else {
		 float alpha = enableFade
			? minAlpha + (baseAlpha - minAlpha) * fadeFactor
			: baseAlpha;
		 color = Vector4(baseGridColor.x, baseGridColor.y, baseGridColor.z, alpha);
	  }

	  // 平面に応じて線を描画
	  Vector3 lineStart, lineEnd;
	  switch (plane) {
		 case GridPlane::XZ:
			lineStart = Vector3(start1, 0.0f, pos2);
			lineEnd = Vector3(end1, 0.0f, pos2);
			break;
		 case GridPlane::XY:
			lineStart = Vector3(start1, pos2, 0.0f);
			lineEnd = Vector3(end1, pos2, 0.0f);
			break;
		 case GridPlane::YZ:
			lineStart = Vector3(0.0f, start1, pos2);
			lineEnd = Vector3(0.0f, end1, pos2);
			break;
	  }

	  DrawLine(lineStart, lineEnd, color, camera);
   }
}

void LineRenderer::DrawSphere(const Vector3& center, float radius, const Vector4& color, Camera* camera) {
   if (!camera) return;

   const int segments = 16;
   const int rings = 8;
   const float pi = 3.14159265358979323846f;

   // 経度方向のライン
   for (int i = 0; i <= rings; ++i) {
	  float theta = pi * static_cast<float>(i) / static_cast<float>(rings);
	  float sinTheta = std::sin(theta);
	  float cosTheta = std::cos(theta);

	  for (int j = 0; j < segments; ++j) {
		 float phi1 = 2.0f * pi * static_cast<float>(j) / static_cast<float>(segments);
		 float phi2 = 2.0f * pi * static_cast<float>(j + 1) / static_cast<float>(segments);

		 Vector3 p1(
			center.x + radius * sinTheta * std::cos(phi1),
			center.y + radius * cosTheta,
			center.z + radius * sinTheta * std::sin(phi1)
		 );

		 Vector3 p2(
			center.x + radius * sinTheta * std::cos(phi2),
			center.y + radius * cosTheta,
			center.z + radius * sinTheta * std::sin(phi2)
		 );

		 DrawLine(p1, p2, color, camera);
	  }
   }

   // 緯度方向のライン
   for (int j = 0; j < segments; ++j) {
	  float phi = 2.0f * pi * static_cast<float>(j) / static_cast<float>(segments);

	  for (int i = 0; i < rings; ++i) {
		 float theta1 = pi * static_cast<float>(i) / static_cast<float>(rings);
		 float theta2 = pi * static_cast<float>(i + 1) / static_cast<float>(rings);

		 Vector3 p1(
			center.x + radius * std::sin(theta1) * std::cos(phi),
			center.y + radius * std::cos(theta1),
			center.z + radius * std::sin(theta1) * std::sin(phi)
		 );

		 Vector3 p2(
			center.x + radius * std::sin(theta2) * std::cos(phi),
			center.y + radius * std::cos(theta2),
			center.z + radius * std::sin(theta2) * std::sin(phi)
		 );

		 DrawLine(p1, p2, color, camera);
	  }
   }
}

void LineRenderer::DrawHemisphere(const Vector3& center, float radius, const Vector3& up, const Vector4& color, Camera* camera) {
   if (!camera) return;

   const int segments = 16;
   const int rings = 8;
   const float pi = 3.14159265358979323846f;

   // 半球のみ描画（theta: 0 to π/2）
   for (int i = 0; i <= rings; ++i) {
	  float theta = pi * 0.5f * static_cast<float>(i) / static_cast<float>(rings);
	  float sinTheta = std::sin(theta);
	  float cosTheta = std::cos(theta);

	  for (int j = 0; j < segments; ++j) {
		 float phi1 = 2.0f * pi * static_cast<float>(j) / static_cast<float>(segments);
		 float phi2 = 2.0f * pi * static_cast<float>(j + 1) / static_cast<float>(segments);

		 Vector3 p1(
			center.x + radius * sinTheta * std::cos(phi1),
			center.y + radius * cosTheta,
			center.z + radius * sinTheta * std::sin(phi1)
		 );

		 Vector3 p2(
			center.x + radius * sinTheta * std::cos(phi2),
			center.y + radius * cosTheta,
			center.z + radius * sinTheta * std::sin(phi2)
		 );

		 DrawLine(p1, p2, color, camera);
	  }
   }

   // ベースの円
   DrawCircle(center, radius, up, color, camera);
}

void LineRenderer::DrawCone(const Vector3& apex, float radius, float height, const Vector3& direction, const Vector4& color, Camera* camera) {
   if (!camera) return;

   const int segments = 16;
   const float pi = 3.14159265358979323846f;

   Vector3 dir = direction.Normalize();
   Vector3 base = apex + dir * height;

   // 基準となる垂直ベクトルを作成
   Vector3 perpendicular = std::abs(dir.y) < 0.9f ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(1.0f, 0.0f, 0.0f);
   Vector3 right = dir.Cross(perpendicular).Normalize();
   Vector3 upVec = right.Cross(dir).Normalize();

   // 円周上の点を計算して頂点から線を引く
   for (int i = 0; i < segments; ++i) {
	  float angle1 = 2.0f * pi * static_cast<float>(i) / static_cast<float>(segments);
	  float angle2 = 2.0f * pi * static_cast<float>(i + 1) / static_cast<float>(segments);

	  Vector3 p1 = base + right * (radius * std::cos(angle1)) + upVec * (radius * std::sin(angle1));
	  Vector3 p2 = base + right * (radius * std::cos(angle2)) + upVec * (radius * std::sin(angle2));

	  // 底面の円
	  DrawLine(p1, p2, color, camera);

	  // 頂点から底面への線
	  DrawLine(apex, p1, color, camera);
   }
}

void LineRenderer::DrawBox(const Vector3& center, const Vector3& size, const Vector4& color, Camera* camera) {
   if (!camera) return;

   Vector3 halfSize = size * 0.5f;

   // 8つの頂点
   Vector3 vertices[8] = {
	   center + Vector3(-halfSize.x, -halfSize.y, -halfSize.z),
	   center + Vector3(halfSize.x, -halfSize.y, -halfSize.z),
	   center + Vector3(halfSize.x,  halfSize.y, -halfSize.z),
	   center + Vector3(-halfSize.x,  halfSize.y, -halfSize.z),
	   center + Vector3(-halfSize.x, -halfSize.y,  halfSize.z),
	   center + Vector3(halfSize.x, -halfSize.y,  halfSize.z),
	   center + Vector3(halfSize.x,  halfSize.y,  halfSize.z),
	   center + Vector3(-halfSize.x,  halfSize.y,  halfSize.z)
   };

   // 12本のエッジ
   // 底面
   DrawLine(vertices[0], vertices[1], color, camera);
   DrawLine(vertices[1], vertices[2], color, camera);
   DrawLine(vertices[2], vertices[3], color, camera);
   DrawLine(vertices[3], vertices[0], color, camera);

   // 上面
   DrawLine(vertices[4], vertices[5], color, camera);
   DrawLine(vertices[5], vertices[6], color, camera);
   DrawLine(vertices[6], vertices[7], color, camera);
   DrawLine(vertices[7], vertices[4], color, camera);

   // 縦のエッジ
   DrawLine(vertices[0], vertices[4], color, camera);
   DrawLine(vertices[1], vertices[5], color, camera);
   DrawLine(vertices[2], vertices[6], color, camera);
   DrawLine(vertices[3], vertices[7], color, camera);
}

void LineRenderer::DrawCircle(const Vector3& center, float radius, const Vector3& normal, const Vector4& color, Camera* camera) {
   if (!camera) return;

   const int segments = 32;
   const float pi = 3.14159265358979323846f;

   Vector3 n = normal.Normalize();

   // 法線に垂直な2つのベクトルを作成
   Vector3 perpendicular = std::abs(n.y) < 0.9f ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(1.0f, 0.0f, 0.0f);
   Vector3 right = n.Cross(perpendicular).Normalize();
   Vector3 upVec = right.Cross(n).Normalize();

   // 円を描画
   for (int i = 0; i < segments; ++i) {
	  float angle1 = 2.0f * pi * static_cast<float>(i) / static_cast<float>(segments);
	  float angle2 = 2.0f * pi * static_cast<float>(i + 1) / static_cast<float>(segments);

	  Vector3 p1 = center + right * (radius * std::cos(angle1)) + upVec * (radius * std::sin(angle1));
	  Vector3 p2 = center + right * (radius * std::cos(angle2)) + upVec * (radius * std::sin(angle2));

	  DrawLine(p1, p2, color, camera);
   }
}

void LineRenderer::Clear() {
   cameraLineGroups_.clear();
}
}