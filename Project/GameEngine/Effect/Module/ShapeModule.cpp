#include "pch.h"
#include "ShapeModule.h"
#include "Object/Model/ModelAsset.h"
#include <random>
#include <cmath>

namespace GameEngine {
namespace {
std::random_device randomDevice;
std::mt19937 randomEngine(randomDevice());

float RandomRange(float min, float max) {
   std::uniform_real_distribution<float> dist(min, max);
   return dist(randomEngine);
}

constexpr float kPi = 3.14159265358979323846f;

Vector3 ResolveSkinnedVertexPosition(
   const ModelAsset& modelAsset,
   const SkinCluster* skinCluster,
   size_t meshIndex,
   uint32_t vertexIndex) {
   const auto& vertices = modelAsset.GetVertices(meshIndex);
   if (vertexIndex >= vertices.size()) {
	  return Vector3(0.0f, 0.0f, 0.0f);
   }
   const Vector3 basePosition(
	  vertices[vertexIndex].position.x,
	  vertices[vertexIndex].position.y,
	  vertices[vertexIndex].position.z);
   if (!skinCluster || meshIndex >= skinCluster->mappedInfluenceData.size() ||
	  !skinCluster->mappedInfluenceData[meshIndex]) {
	  return basePosition;
   }

   const VertexInfluence& influence = skinCluster->mappedInfluenceData[meshIndex][vertexIndex];
   Vector3 skinnedPosition(0.0f, 0.0f, 0.0f);
   float totalWeight = 0.0f;
   for (uint32_t influenceIndex = 0; influenceIndex < kNumMaxInfluence; ++influenceIndex) {
	  const float weight = influence.weights[influenceIndex];
	  const int32_t jointIndex = influence.jointIndices[influenceIndex];
	  if (weight <= 0.0f || jointIndex < 0 || static_cast<size_t>(jointIndex) >= skinCluster->mappedPalette.size()) {
		 continue;
	  }
	  skinnedPosition += TransformCoordinate(
		 basePosition,
		 skinCluster->mappedPalette[static_cast<size_t>(jointIndex)].skeletonSpaceMatrix) * weight;
	  totalWeight += weight;
   }
   return totalWeight > 0.0001f ? skinnedPosition / totalWeight : basePosition;
}
}

ShapeModule::ShapeModule() = default;

Vector3 ShapeModule::GetRandomEmissionPosition() const {
   Vector3 localOffset{ 0.0f, 0.0f, 0.0f };
   const Vector3& scale = transform_.scale;

   switch (shapeType_) {
	  case ShapeType::Sphere: {
		 float theta = RandomRange(0.0f, 2.0f * kPi);
		 float phi = RandomRange(0.0f, kPi);
		 float r = (emitFrom_ == EmitFrom::Shell) ? radius_ : RandomRange(0.0f, radius_);
		 localOffset.x += r * std::sin(phi) * std::cos(theta) * scale.x;
		 localOffset.y += r * std::cos(phi) * scale.y;
		 localOffset.z += r * std::sin(phi) * std::sin(theta) * scale.z;
		 break;
	  }
	  case ShapeType::Hemisphere: {
		 float theta = RandomRange(0.0f, 2.0f * kPi);
		 float phi = RandomRange(0.0f, kPi * 0.5f);
		 float r = (emitFrom_ == EmitFrom::Shell) ? radius_ : RandomRange(0.0f, radius_);
		 localOffset.x += r * std::sin(phi) * std::cos(theta) * scale.x;
		 localOffset.y += r * std::cos(phi) * scale.y;
		 localOffset.z += r * std::sin(phi) * std::sin(theta) * scale.z;
		 break;
	  }
	  case ShapeType::Cone: {
		 float angle = RandomRange(0.0f, angle_ * kPi / 180.0f);
		 float rotation = RandomRange(0.0f, 2.0f * kPi);
		 float distance = RandomRange(0.0f, length_);
		 localOffset.x += distance * std::sin(angle) * std::cos(rotation) * scale.x;
		 localOffset.y += distance * std::cos(angle) * scale.y;
		 localOffset.z += distance * std::sin(angle) * std::sin(rotation) * scale.z;
		 break;
	  }
	  case ShapeType::Box: {
		 if (emitFrom_ == EmitFrom::Volume) {
			localOffset.x += RandomRange(-boxSize_.x * 0.5f, boxSize_.x * 0.5f) * scale.x;
			localOffset.y += RandomRange(-boxSize_.y * 0.5f, boxSize_.y * 0.5f) * scale.y;
			localOffset.z += RandomRange(-boxSize_.z * 0.5f, boxSize_.z * 0.5f) * scale.z;
		 } else {
			int face = static_cast<int>(RandomRange(0.0f, 6.0f));
			switch (face) {
			   case 0: localOffset.x += boxSize_.x * 0.5f * scale.x; break;
			   case 1: localOffset.x -= boxSize_.x * 0.5f * scale.x; break;
			   case 2: localOffset.y += boxSize_.y * 0.5f * scale.y; break;
			   case 3: localOffset.y -= boxSize_.y * 0.5f * scale.y; break;
			   case 4: localOffset.z += boxSize_.z * 0.5f * scale.z; break;
			   case 5: localOffset.z -= boxSize_.z * 0.5f * scale.z; break;
			}
		 }
		 break;
	  }
	  case ShapeType::Circle: {
		 float angle = RandomRange(0.0f, arc_ * kPi / 180.0f);
		 float r = (emitFrom_ == EmitFrom::Edge) ? radius_ : RandomRange(0.0f, radius_);
		 localOffset.x += r * std::cos(angle) * scale.x;
		 localOffset.z += r * std::sin(angle) * scale.z;
		 break;
	  }
	  case ShapeType::Edge: {
		 float t = RandomRange(0.0f, 1.0f);
		 localOffset.x += (t - 0.5f) * scale.x;
		 break;
	  }
	  case ShapeType::Cylinder: {
		 const float angle = RandomRange(0.0f, 2.0f * kPi);
		 const float radialDistance = emitFrom_ == EmitFrom::Volume
			? std::sqrt(RandomRange(0.0f, 1.0f)) * radius_
			: radius_;
		 localOffset.x += radialDistance * std::cos(angle) * scale.x;
		 localOffset.y += RandomRange(-length_ * 0.5f, length_ * 0.5f) * scale.y;
		 localOffset.z += radialDistance * std::sin(angle) * scale.z;
		 break;
	  }
	  case ShapeType::Torus: {
		 const float majorAngle = RandomRange(0.0f, 2.0f * kPi);
		 const float minorAngle = RandomRange(0.0f, 2.0f * kPi);
		 const float minorRadius = emitFrom_ == EmitFrom::Volume
			? std::sqrt(RandomRange(0.0f, 1.0f)) * radius_
			: radius_;
		 const float radialDistance = torusMajorRadius_ + minorRadius * std::cos(minorAngle);
		 localOffset.x += radialDistance * std::cos(majorAngle) * scale.x;
		 localOffset.y += minorRadius * std::sin(minorAngle) * scale.y;
		 localOffset.z += radialDistance * std::sin(majorAngle) * scale.z;
		 break;
	  }
	  case ShapeType::SkinnedMesh: {
		 if (!skinnedMeshModel_) {
			break;
		 }
		 const auto& meshes = skinnedMeshModel_->GetMeshData();
		 size_t totalTriangleCount = 0;
		 for (const auto& mesh : meshes) totalTriangleCount += mesh.indices.size() / 3;
		 if (totalTriangleCount == 0) {
			break;
		 }
		 size_t selectedTriangle = (std::min)(
			static_cast<size_t>(RandomRange(0.0f, static_cast<float>(totalTriangleCount))),
			totalTriangleCount - 1);
		 size_t meshIndex = 0;
		 while (meshIndex < meshes.size()) {
			const size_t meshTriangleCount = meshes[meshIndex].indices.size() / 3;
			if (selectedTriangle < meshTriangleCount) break;
			selectedTriangle -= meshTriangleCount;
			++meshIndex;
		 }
		 if (meshIndex >= meshes.size()) break;
		 const auto& indices = meshes[meshIndex].indices;
		 const size_t indexOffset = selectedTriangle * 3;
		 const Vector3 p0 = ResolveSkinnedVertexPosition(*skinnedMeshModel_, skinnedMeshSkinCluster_, meshIndex, indices[indexOffset]);
		 const Vector3 p1 = ResolveSkinnedVertexPosition(*skinnedMeshModel_, skinnedMeshSkinCluster_, meshIndex, indices[indexOffset + 1]);
		 const Vector3 p2 = ResolveSkinnedVertexPosition(*skinnedMeshModel_, skinnedMeshSkinCluster_, meshIndex, indices[indexOffset + 2]);
		 const float sqrtRandom = std::sqrt(RandomRange(0.0f, 1.0f));
		 const float barycentric1 = RandomRange(0.0f, 1.0f);
		 localOffset = p0 * (1.0f - sqrtRandom) +
			p1 * (sqrtRandom * (1.0f - barycentric1)) +
			p2 * (sqrtRandom * barycentric1);
		 lastEmissionDirection_ = (p1 - p0).Cross(p2 - p0).Normalize();
		 localOffset = Vector3(localOffset.x * scale.x, localOffset.y * scale.y, localOffset.z * scale.z);
		 break;
	  }
	  case ShapeType::Point:
	  default:
		 break;
   }

	  const Quaternion shapeRotation = transform_.GetActiveQuaternion();
   return transform_.translation + RotateVector(localOffset, shapeRotation);
}

Vector3 ShapeModule::GetRandomEmissionDirection() const {
   Vector3 direction{ 0.0f, 1.0f, 0.0f };

   switch (shapeType_) {
	  case ShapeType::Sphere:
	  case ShapeType::Hemisphere: {
		 float theta = RandomRange(0.0f, 2.0f * kPi);
		 float phi = (shapeType_ == ShapeType::Hemisphere)
			? RandomRange(0.0f, kPi * 0.5f)
			: RandomRange(0.0f, kPi);
		 direction.x = std::sin(phi) * std::cos(theta);
		 direction.y = std::cos(phi);
		 direction.z = std::sin(phi) * std::sin(theta);
		 break;
	  }
	  case ShapeType::Cone: {
		 float angle = RandomRange(0.0f, angle_ * kPi / 180.0f);
		 float rotation = RandomRange(0.0f, 2.0f * kPi);
		 direction.x = std::sin(angle) * std::cos(rotation);
		 direction.y = std::cos(angle);
		 direction.z = std::sin(angle) * std::sin(rotation);
		 break;
	  }
	  case ShapeType::Cylinder: {
		 const float angle = RandomRange(0.0f, 2.0f * kPi);
		 direction = Vector3(std::cos(angle), 0.0f, std::sin(angle));
		 break;
	  }
	  case ShapeType::Torus: {
		 const float majorAngle = RandomRange(0.0f, 2.0f * kPi);
		 const float minorAngle = RandomRange(0.0f, 2.0f * kPi);
		 direction = Vector3(
			std::cos(minorAngle) * std::cos(majorAngle),
			std::sin(minorAngle),
			std::cos(minorAngle) * std::sin(majorAngle));
		 break;
	  }
	  case ShapeType::SkinnedMesh:
		 direction = lastEmissionDirection_;
		 break;
	  default:
		 break;
   }

	  const Quaternion shapeRotation = transform_.GetActiveQuaternion();
   return RotateVector(direction.Normalize(), shapeRotation).Normalize();
}

Vector3 ShapeModule::GetCircleOutwardDirection(const Vector3& emissionPosition) const {
   const Quaternion shapeRotation = transform_.GetActiveQuaternion();
   const Vector3 circleNormal = RotateVector(Vector3(0.0f, 1.0f, 0.0f), shapeRotation).Normalize();

   Vector3 outward = emissionPosition - transform_.translation;
   outward = outward - circleNormal * outward.Dot(circleNormal);

   if (outward.LengthSquared() < 1e-8f) {
	  outward = RotateVector(Vector3(1.0f, 0.0f, 0.0f), shapeRotation);
   }

   return outward.Normalize();
}

nlohmann::json ShapeModule::ToJson() const {
   nlohmann::json j;

   j["enabled"] = enabled_;
   j["shapeType"] = static_cast<int>(shapeType_);
   j["emitFrom"] = static_cast<int>(emitFrom_);
   j["radius"] = radius_;
   j["angle"] = angle_;
   j["length"] = length_;
   j["boxSize"] = { boxSize_.x, boxSize_.y, boxSize_.z };
   j["arc"] = arc_;
   j["circleOutwardVelocity"] = circleOutwardVelocity_;
   j["torusMajorRadius"] = torusMajorRadius_;
   j["position"] = { transform_.translation.x, transform_.translation.y, transform_.translation.z };
   const Vector3 activeEuler = transform_.GetActiveEuler();
   j["rotation"] = { activeEuler.x, activeEuler.y, activeEuler.z };
   j["scale"] = { transform_.scale.x, transform_.scale.y, transform_.scale.z };

   return j;
}

void ShapeModule::FromJson(const nlohmann::json& j) {
   if (j.contains("enabled")) enabled_ = j["enabled"];
   if (j.contains("shapeType")) shapeType_ = static_cast<ShapeType>(j["shapeType"].get<int>());
   if (j.contains("emitFrom")) emitFrom_ = static_cast<EmitFrom>(j["emitFrom"].get<int>());
   if (j.contains("radius")) radius_ = j["radius"];
   if (j.contains("angle")) angle_ = j["angle"];
   if (j.contains("length")) length_ = j["length"];

   if (j.contains("boxSize")) {
	  auto arr = j["boxSize"];
	  boxSize_ = Vector3{ arr[0], arr[1], arr[2] };
   }

   if (j.contains("arc")) arc_ = j["arc"];
   if (j.contains("circleOutwardVelocity")) circleOutwardVelocity_ = j["circleOutwardVelocity"];
   if (j.contains("torusMajorRadius")) SetTorusMajorRadius(j["torusMajorRadius"]);

   if (j.contains("position")) {
	  auto arr = j["position"];
	  transform_.translation = Vector3{ arr[0], arr[1], arr[2] };
   }
   if (j.contains("rotation")) {
	  auto arr = j["rotation"];
	  transform_.SetRotationQuaternion(Vector3ToQuaternion(Vector3(arr[0], arr[1], arr[2])));
   }
   if (j.contains("scale")) {
	  auto arr = j["scale"];
	  transform_.scale = Vector3{ arr[0], arr[1], arr[2] };
   }
}
}
