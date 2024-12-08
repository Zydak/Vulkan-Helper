#include "pch.h"
#include "AssetImporter.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "AssetManager.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

namespace VulkanHelper
{
	Image AssetImporter::ImportTexture(std::string path, bool HDR)
	{
		for (int i = 0; i < path.size(); i++)
		{
			if (path[i] == '%')
				path[i] = ' ';
		}

		int texChannels;
		bool flipOnLoad = !HDR;
		stbi_set_flip_vertically_on_load_thread(flipOnLoad);
		int sizeX, sizeY;
		void* pixels;
		if (HDR)
		{
			pixels = stbi_loadf(path.c_str(), &sizeX, &sizeY, &texChannels, STBI_rgb_alpha);
		}
		else
		{
			pixels = stbi_load(path.c_str(), &sizeX, &sizeY, &texChannels, STBI_rgb_alpha);
		}

		std::filesystem::path cwd = std::filesystem::current_path();
		VK_CORE_ASSERT(pixels, "failed to load texture image! Path: {0}, Current working directory: {1}", path, cwd.string());
		uint64_t sizeOfPixel = HDR ? sizeof(float) * 4 : sizeof(uint8_t) * 4;
		VkDeviceSize imageSize = (uint64_t)sizeX * (uint64_t)sizeY * sizeOfPixel;

		Image::CreateInfo info{};
		info.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		info.Format = HDR ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;
		info.Height = sizeY;
		info.Width = sizeX;
		info.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		info.Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		info.Data = (void*)pixels;
		info.HDR = HDR;
		info.MipMapCount = glm::min(5, (int)glm::floor(glm::log2((float)glm::max(sizeX, sizeY))));
		Image image(info);

		stbi_image_free(pixels);

		return Image(std::move(image));
	}

	ModelAsset AssetImporter::ImportModel(const std::string& path)
	{
		Timer timer;

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(path,
			aiProcess_CalcTangentSpace |
			aiProcess_GenSmoothNormals |
			aiProcess_ImproveCacheLocality |
			aiProcess_RemoveRedundantMaterials |
			aiProcess_SplitLargeMeshes |
			aiProcess_Triangulate |
			aiProcess_GenUVCoords |
			aiProcess_SortByPType |
			aiProcess_FindDegenerates |
			aiProcess_FindInvalidData);
		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			VK_CORE_ERROR("Failed to load model: {0}", importer.GetErrorString());
			VK_CORE_ASSERT(false, ""); // TODO: some error handling
		}

		ModelAsset asset;

		int index = 0;
		ProcessAssimpNode(scene->mRootNode, scene, path, &asset, index);

		for (int i = 0; i < asset.Meshes.size(); i++)
		{
			asset.Meshes[i].WaitToLoad();
		}
		for (int i = 0; i < asset.Materials.size(); i++)
		{
			asset.Materials[i].WaitToLoad();
		}

		return asset;
	}

	void AssetImporter::ProcessAssimpNode(aiNode* node, const aiScene* scene, const std::string& filepath, ModelAsset* outAsset, int& index)
	{
		// process each mesh located at the current node
		for (unsigned int i = 0; i < node->mNumMeshes; i++)
		{
			glm::mat4 transform = glm::transpose(*(glm::mat4*)(&node->mTransformation));
			aiNode* currNode = node;
			while (true)
			{
				if (currNode->mParent)
				{
					currNode = currNode->mParent;
					transform = glm::transpose(*(glm::mat4*)(&currNode->mTransformation)) * transform;
				}
				else
				{
					break;
				}
			}

			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			std::string meshName = node->mName.C_Str();

			{
				std::string path = filepath + "::Mesh::" + meshName;

				std::hash<std::string> hash;

				// For some models multiple meshes have the same name (for example due to using multiple materials on a single mesh in blender),
				// otherwise the incorrect mesh will be used
				int indexMesh = 0;
				while (true)
				{
					AssetHandle handle(AssetHandle::CreateInfo{ hash(path + std::to_string(indexMesh)) });
					if (handle.DoesHandleExist())
						indexMesh++;
					else break;
				}
				path += std::to_string(indexMesh);

				Mesh vlMesh(mesh, scene);

				std::unique_ptr<Asset> meshAsset = std::make_unique<MeshAsset>(std::move(vlMesh));
				AssetHandle handle = AssetManager::AddAsset(path, std::move(meshAsset));

				outAsset->MeshNames.push_back(meshName);
				outAsset->Meshes.push_back(handle);
			}

			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
			Material mat;

			aiColor4D emissiveColor(0.0f, 0.0f, 0.0f, 0.0f);
			aiColor4D diffuseColor(0.0f, 0.0f, 0.0f, 0.0f);

			{
				std::string matName = material->GetName().C_Str();

				mat.MaterialName = matName;

				std::string path = filepath + "::Material::" + matName;

				std::hash<std::string> hash;
				AssetHandle handle(AssetHandle::CreateInfo{ hash(path) });

				if (!handle.DoesHandleExist())
				{
					material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor);
					material->Get(AI_MATKEY_EMISSIVE_INTENSITY, emissiveColor.a);
					material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor);
					material->Get(AI_MATKEY_ROUGHNESS_FACTOR, mat.Properties.Roughness);
					material->Get(AI_MATKEY_METALLIC_FACTOR, mat.Properties.Metallic);
					material->Get(AI_MATKEY_REFRACTI, mat.Properties.Ior);

					mat.Properties.Roughness = glm::pow(mat.Properties.Roughness, 1.0f / 4.0f);

					for (int i = 0; i < (int)material->GetTextureCount(aiTextureType_DIFFUSE); i++)
					{
						aiString str;
						material->GetTexture(aiTextureType_DIFFUSE, i, &str);
						mat.Textures.SetAlbedo(AssetManager::LoadAsset(std::string("assets/") + std::string(str.C_Str())));
					}

					for (int i = 0; i < (int)material->GetTextureCount(aiTextureType_NORMALS); i++)
					{
						aiString str;
						material->GetTexture(aiTextureType_NORMALS, i, &str);
						mat.Textures.SetNormal(AssetManager::LoadAsset(std::string("assets/") + std::string(str.C_Str())));
					}

					for (int i = 0; i < (int)material->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS); i++)
					{
						aiString str;
						material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, i, &str);
						mat.Textures.SetRoughness(AssetManager::LoadAsset(std::string("assets/") + std::string(str.C_Str())));
					}

					for (int i = 0; i < (int)material->GetTextureCount(aiTextureType_METALNESS); i++)
					{
						aiString str;
						material->GetTexture(aiTextureType_METALNESS, i, &str);
						mat.Textures.SetMetallness(AssetManager::LoadAsset(std::string("assets/") + std::string(str.C_Str())));
					}

					// Create Empty Texture if none are found
					if (material->GetTextureCount(aiTextureType_DIFFUSE) == 0)
					{
						mat.Textures.SetAlbedo(AssetManager::LoadAsset("assets/white.png"));
					}
					if (material->GetTextureCount(aiTextureType_NORMALS) == 0)
					{
						mat.Textures.SetNormal(AssetManager::LoadAsset("assets/empty_normal.png"));
					}
					if (material->GetTextureCount(aiTextureType_METALNESS) == 0)
					{
						mat.Textures.SetMetallness(AssetManager::LoadAsset("assets/white.png"));
					}
					if (material->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) == 0)
					{
						mat.Textures.SetRoughness(AssetManager::LoadAsset("assets/white.png"));
					}

					mat.Properties.Color = glm::vec4(diffuseColor.r, diffuseColor.g, diffuseColor.b, 1.0f);
					mat.Properties.EmissiveColor = glm::vec4(emissiveColor.r, emissiveColor.g, emissiveColor.b, emissiveColor.a);

					mat.Properties.Transparency = 1.0f - diffuseColor.a;

					std::unique_ptr<Asset> materialAsset = std::make_unique<MaterialAsset>(std::move(mat));
					handle = AssetManager::AddAsset(path, std::move(materialAsset));
				}

				outAsset->Materials.push_back(handle);
			}

			// Rotate every model 180 degrees
			glm::mat4 rot = glm::rotate(glm::mat4{ 1.0f }, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			transform = rot * transform;
			outAsset->MeshTransfrorms.push_back(transform);

			index++;
		}

		// process each of the children nodes
		for (unsigned int i = 0; i < node->mNumChildren; i++)
		{
			ProcessAssimpNode(node->mChildren[i], scene, filepath, outAsset, index);
		}
	}

}