#include "pch.h"
#include "ModelAsset.h"
#include "ResourceHelper.h"
#include "Graphics/GraphicsDevice.h"
#include <cassert>
#include <algorithm>
#include <cstring>
#include <string_view>
#include "MathUtils.h"

namespace GameEngine {
void ModelAsset::LoadFile(GraphicsDevice* device, const std::string& modelPath, const std::string& modelName) {
   assert(device);
   graphicsDevice_ = device;
   ID3D12Device* d3dDevice = device->GetDevice();

   hasSkinningData_ = false;
   modelData_ = LoadModelFile(modelPath, modelName);
   skeleton_ = CreateSkeleton(modelData_.rootNode, modelData_);
   skinCluster_.reset();

   vertexResources_.resize(modelData_.meshes.size());
   vertexBufferViews_.resize(modelData_.meshes.size());
   mappedVertexData_.resize(modelData_.meshes.size());
   indexResources_.resize(modelData_.meshes.size());
   indexBufferViews_.resize(modelData_.meshes.size());
   mappedIndexData_.resize(modelData_.meshes.size());

   for (size_t i = 0; i < modelData_.meshes.size(); ++i) {
	  const auto& mesh = modelData_.meshes[i];
    vertexResources_[i] = ResourceHelper::CreateBufferResource(d3dDevice, sizeof(Mesh::VertexData) * mesh.vertices.size());

	  vertexBufferViews_[i].BufferLocation = vertexResources_[i]->GetGPUVirtualAddress();
	  vertexBufferViews_[i].SizeInBytes = static_cast<UINT>(sizeof(Mesh::VertexData) * mesh.vertices.size());
	  vertexBufferViews_[i].StrideInBytes = sizeof(Mesh::VertexData);

	  vertexResources_[i]->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertexData_[i]));
	  std::memcpy(mappedVertexData_[i], mesh.vertices.data(), sizeof(Mesh::VertexData) * mesh.vertices.size());

      indexResources_[i] = ResourceHelper::CreateBufferResource(d3dDevice, sizeof(uint32_t) * mesh.indices.size());
	  indexBufferViews_[i].BufferLocation = indexResources_[i]->GetGPUVirtualAddress();
	  indexBufferViews_[i].Format = DXGI_FORMAT_R32_UINT;
	  indexBufferViews_[i].SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * mesh.indices.size());

	  indexResources_[i]->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndexData_[i]));
	  std::memcpy(mappedIndexData_[i], mesh.indices.data(), sizeof(uint32_t) * mesh.indices.size());
   }

   if (hasSkinningData_ && skeleton_ && !skeleton_->joints.empty() && !modelData_.meshes.empty()) {
	  skinCluster_ = CreateSkinCluster(device, *skeleton_, modelData_);
   }
}

std::optional<SkinCluster> ModelAsset::CreateSkinClusterInstance() {
   if (!hasSkinningData_ || !graphicsDevice_ || !skeleton_ || !skinCluster_) {
	  return std::nullopt;
   }

   SkinCluster instance = *skinCluster_;
   ID3D12Device* d3dDevice = graphicsDevice_->GetDevice();

   instance.paletteResource = ResourceHelper::CreateBufferResource(d3dDevice, sizeof(WellForGPU) * skeleton_->joints.size());
   WellForGPU* mappedPalette = nullptr;
   instance.paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
   instance.mappedPalette = { mappedPalette, skeleton_->joints.size() };

   const UINT index = graphicsDevice_->GetNextSrvIndex();
   instance.paletteSrvHandle.first = CD3DX12_CPU_DESCRIPTOR_HANDLE(
	  graphicsDevice_->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart(),
	  index,
	  graphicsDevice_->GetDescriptorSizeCBVSRVUAV());
   instance.paletteSrvHandle.second = CD3DX12_GPU_DESCRIPTOR_HANDLE(
	  graphicsDevice_->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart(),
	  index,
	  graphicsDevice_->GetDescriptorSizeCBVSRVUAV());

   D3D12_SHADER_RESOURCE_VIEW_DESC paletteSrvDesc{};
   paletteSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
   paletteSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   paletteSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
   paletteSrvDesc.Buffer.FirstElement = 0;
   paletteSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
   paletteSrvDesc.Buffer.NumElements = static_cast<UINT>(skeleton_->joints.size());
   paletteSrvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);
   d3dDevice->CreateShaderResourceView(instance.paletteResource.Get(), &paletteSrvDesc, instance.paletteSrvHandle.first);
   graphicsDevice_->IncrementSrvIndex();

   for (size_t jointIndex = 0; jointIndex < skeleton_->joints.size(); ++jointIndex) {
	  const Matrix4x4 skinMatrix = instance.inverseBindPoseMatrices[jointIndex] * skeleton_->joints[jointIndex].skeletonSpaceMatrix;
	  instance.mappedPalette[jointIndex].skeletonSpaceMatrix = skinMatrix;
	  instance.mappedPalette[jointIndex].skeletonSpaceInverseTransposeMatrix = skinMatrix.Inverse().Transpose();
   }

   return instance;
}

ModelData ModelAsset::LoadModelFile(const std::string& directoryPath, const std::string& filename) {
   ModelData modelData;

   Assimp::Importer importer;
   std::string filePath = directoryPath + "/" + filename;
   const aiScene* scene = importer.ReadFile(filePath.c_str(),
	  aiProcess_FlipWindingOrder |
	  aiProcess_FlipUVs |
	  aiProcess_Triangulate
   );
   assert(scene->HasMeshes());

   for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
	  aiMesh* mesh = scene->mMeshes[meshIndex];
	  assert(mesh->HasNormals());
	  hasSkinningData_ = hasSkinningData_ || mesh->HasBones();

	  MeshData meshData;
	  meshData.materialIndex = mesh->mMaterialIndex;

	  meshData.vertices.reserve(mesh->mNumVertices);
	  for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
		 aiVector3D& position = mesh->mVertices[vertexIndex];
		 aiVector3D& normal = mesh->mNormals[vertexIndex];

		 Mesh::VertexData vertex;
		 vertex.position = Vector4(-position.x, position.y, position.z, 1.0f);
		 vertex.normal = Vector3(-normal.x, normal.y, normal.z);

		 if (mesh->HasTextureCoords(0)) {
			aiVector3D& texCoord = mesh->mTextureCoords[0][vertexIndex];
			vertex.texCoord = Vector2(texCoord.x, texCoord.y);
		 } else {
			vertex.texCoord = Vector2(0.0f, 0.0f);
		 }

		 meshData.vertices.push_back(vertex);
	  }

	  meshData.indices.reserve(mesh->mNumFaces * 3);

	  for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
		 aiFace& face = mesh->mFaces[faceIndex];
		 assert(face.mNumIndices == 3);

		 for (uint32_t element = 0; element < face.mNumIndices; ++element) {
			meshData.indices.push_back(face.mIndices[element]);
		 }
	  }

	  modelData.meshes.push_back(meshData);

	  for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
		 aiBone* bone = mesh->mBones[boneIndex];
		 std::string jointName = bone->mName.C_Str();
		 JointWeightData& jointWeightData = modelData.skinClusterData[jointName];

		 aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse();
		 aiVector3D scale, translation;
		 aiQuaternion rotation;
		 bindPoseMatrixAssimp.Decompose(scale, rotation, translation);
		 Transform bindPoseTransform;
		 bindPoseTransform.scale = Vector3(scale.x, scale.y, scale.z);
		 bindPoseTransform.translation = Vector3(-translation.x, translation.y, translation.z);
		 bindPoseTransform.SetRotationQuaternion(Quaternion(rotation.x, -rotation.y, -rotation.z, rotation.w));
		 Matrix4x4 bindPoseMatrix = MakeAffineMatrix(bindPoseTransform);
		 jointWeightData.inverseBindPoseMatrix = bindPoseMatrix.Inverse();

        for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
			jointWeightData.vertexWeights.push_back({
			   bone->mWeights[weightIndex].mWeight,
			   bone->mWeights[weightIndex].mVertexId,
			   meshIndex
			});
		 }
	  }

   }

   for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
	  MaterialAsset materialAsset{};
	  aiMaterial* material = scene->mMaterials[materialIndex];
	  if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
		 aiString textureFilePath;
		 material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
		 materialAsset.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
	  }
	  modelData.materials.push_back(materialAsset);
   }

   modelData.rootNode = ReadNode(scene->mRootNode);
   return modelData;
}

Node ModelAsset::ReadNode(aiNode* node) {
   Node result;
   aiMatrix4x4 aiLocalMatrix = node->mTransformation;
   aiLocalMatrix.Transpose();

   aiVector3D scale, translation;
   aiQuaternion rotation;
   node->mTransformation.Decompose(scale, rotation, translation);
   result.transform.scale = Vector3(scale.x, scale.y, scale.z);
   result.transform.translation = Vector3(-translation.x, translation.y, translation.z);
   result.transform.SetRotationQuaternion(Quaternion(rotation.x, -rotation.y, -rotation.z, rotation.w));
   result.localMatrix = MakeAffineMatrix(result.transform);

   result.name = node->mName.C_Str();
   result.children.resize(node->mNumChildren);
   for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
	  result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
   }
   return result;
}

Skeleton ModelAsset::CreateSkeleton(const Node& rootNode, const ModelData& modelData) {
   (void)modelData;
   Skeleton skeleton;
   skeleton.root = CreateJoint(rootNode, {}, skeleton.joints);

   for (const Joint& joint : skeleton.joints) {
	  skeleton.jointMap.emplace(joint.name, joint.index);
   }

   skeleton.Update();

   return skeleton;
}

int32_t ModelAsset::CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints) {
   Joint joint;
   joint.name = node.name;
   joint.localMatrix = node.localMatrix;
   joint.skeletonSpaceMatrix = MakeIdentity4x4();
   joint.transform = node.transform;
   joint.index = static_cast<int32_t>(joints.size());
   joint.parent = parent;
   joints.push_back(joint);
   for (const Node& child : node.children) {
	  int32_t childIndex = CreateJoint(child, joint.index, joints);
	  joints[joint.index].children.push_back(childIndex);
   }

   return joint.index;

}

SkinCluster ModelAsset::CreateSkinCluster(GraphicsDevice* device, const Skeleton& skeleton, const ModelData& modelData) {
   SkinCluster skinCluster;

   if (!device) {
	  return skinCluster;
   }

   ID3D12Device* d3dDevice = device->GetDevice();

   if (modelData.meshes.empty()) {
	  return skinCluster;
   }

   // 1) palette用Resourceを確保
   skinCluster.paletteResource = ResourceHelper::CreateBufferResource(d3dDevice, sizeof(WellForGPU) * skeleton.joints.size());
   WellForGPU* mappedPalette = nullptr;
   skinCluster.paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
   skinCluster.mappedPalette = { mappedPalette, skeleton.joints.size() };

   // 2) palette用SRVを作成
   {
	  const UINT index = device->GetNextSrvIndex();
	  skinCluster.paletteSrvHandle.first = CD3DX12_CPU_DESCRIPTOR_HANDLE(
		 device->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart(),
		 index,
         device->GetDescriptorSizeCBVSRVUAV());
	  skinCluster.paletteSrvHandle.second = CD3DX12_GPU_DESCRIPTOR_HANDLE(
         device->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart(),
		 index,
         device->GetDescriptorSizeCBVSRVUAV());

	  D3D12_SHADER_RESOURCE_VIEW_DESC paletteSrvDesc{};
	  paletteSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	  paletteSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	  paletteSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	  paletteSrvDesc.Buffer.FirstElement = 0;
	  paletteSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	  paletteSrvDesc.Buffer.NumElements = static_cast<UINT>(skeleton.joints.size());
	  paletteSrvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);
      d3dDevice->CreateShaderResourceView(
		 skinCluster.paletteResource.Get(),
		 &paletteSrvDesc,
		 skinCluster.paletteSrvHandle.first);

      device->IncrementSrvIndex();
   }

   // 3) influence用Resourceを確保
   skinCluster.influenceResources.resize(modelData.meshes.size());
   skinCluster.influenceBufferViews.resize(modelData.meshes.size());
   skinCluster.mappedInfluenceData.resize(modelData.meshes.size(), nullptr);

   // 4) influence用VBVを作成
   for (size_t meshIndex = 0; meshIndex < modelData.meshes.size(); ++meshIndex) {
	  const auto& mesh = modelData.meshes[meshIndex];
	  skinCluster.influenceResources[meshIndex] = ResourceHelper::CreateBufferResource(d3dDevice, sizeof(VertexInfluence) * mesh.vertices.size());

	  VertexInfluence* mappedInfluence = nullptr;
	  skinCluster.influenceResources[meshIndex]->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluence));
	  skinCluster.mappedInfluenceData[meshIndex] = mappedInfluence;

	  if (mappedInfluence && !mesh.vertices.empty()) {
		 std::memset(mappedInfluence, 0, sizeof(VertexInfluence) * mesh.vertices.size());
	  }

	  for (size_t vertexIndex = 0; vertexIndex < mesh.vertices.size(); ++vertexIndex) {
		 auto& influence = mappedInfluence[vertexIndex];
		 influence.jointIndices.fill(-1);
	  }

	  skinCluster.influenceBufferViews[meshIndex].BufferLocation = skinCluster.influenceResources[meshIndex]->GetGPUVirtualAddress();
	  skinCluster.influenceBufferViews[meshIndex].SizeInBytes = static_cast<UINT>(sizeof(VertexInfluence) * mesh.vertices.size());
	  skinCluster.influenceBufferViews[meshIndex].StrideInBytes = sizeof(VertexInfluence);
   }

   // 5) InverseBindPoseMatrixの保存領域を作成
   skinCluster.inverseBindPoseMatrices.resize(skeleton.joints.size());
   std::generate(skinCluster.inverseBindPoseMatrices.begin(), skinCluster.inverseBindPoseMatrices.end(), MakeIdentity4x4);

   // 6) ModelDataのSkinCluster情報を解析してInfluenceの中身を埋める
   for (const auto& jointWeight : modelData.skinClusterData) {
	  auto it = skeleton.jointMap.find(jointWeight.first);
	  if (it == skeleton.jointMap.end()) {
		 continue;
	  }

	  const int32_t jointIndex = it->second;
	  if (jointIndex < 0 || static_cast<size_t>(jointIndex) >= skinCluster.inverseBindPoseMatrices.size()) {
		 continue;
	  }

	  skinCluster.inverseBindPoseMatrices[static_cast<size_t>(jointIndex)] = jointWeight.second.inverseBindPoseMatrix;

	  for (const auto& vertexWeight : jointWeight.second.vertexWeights) {
		 if (vertexWeight.meshIndex >= modelData.meshes.size()) {
			continue;
		 }

		 const auto& mesh = modelData.meshes[vertexWeight.meshIndex];
		 if (vertexWeight.vertexId >= mesh.vertices.size()) {
			continue;
		 }

		 VertexInfluence* mappedInfluence = skinCluster.mappedInfluenceData[vertexWeight.meshIndex];
		 if (!mappedInfluence) {
			continue;
		 }

		 auto& influence = mappedInfluence[vertexWeight.vertexId];
		 for (uint32_t index = 0; index < kNumMaxInfluence; ++index) {
			 if (influence.weights[index] == 0.0f) {
				influence.weights[index] = vertexWeight.weight;
				influence.jointIndices[index] = jointIndex;
				break;
			 }
		 }
	  }
   }

   for (size_t meshIndex = 0; meshIndex < modelData.meshes.size(); ++meshIndex) {
	  const auto& mesh = modelData.meshes[meshIndex];
	  VertexInfluence* mappedInfluence = skinCluster.mappedInfluenceData[meshIndex];
	  if (!mappedInfluence) {
		 continue;
	  }

	  for (size_t vertexIndex = 0; vertexIndex < mesh.vertices.size(); ++vertexIndex) {
		 auto& influence = mappedInfluence[vertexIndex];
		 float totalWeight = 0.0f;
		 for (float weight : influence.weights) {
			totalWeight += weight;
		 }

		 if (totalWeight > 0.0f) {
			const float invWeight = 1.0f / totalWeight;
			for (float& weight : influence.weights) {
			   weight *= invWeight;
			}
		 }
	  }
   }

   for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
	  const Matrix4x4 skinMatrix = skinCluster.inverseBindPoseMatrices[jointIndex] * skeleton.joints[jointIndex].skeletonSpaceMatrix;
	  skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix = skinMatrix;
	  skinCluster.mappedPalette[jointIndex].skeletonSpaceInverseTransposeMatrix = skinMatrix.Inverse().Transpose();
   }

   return skinCluster;
}
}