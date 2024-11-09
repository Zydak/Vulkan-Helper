#include "pch.h"
#include "Components.h"
#include "Renderer/Renderer.h"

#include "Asset/Serializer.h"
#include "Asset/AssetManager.h"

namespace VulkanHelper
{
	void ScriptComponent::InitializeScripts()
	{
		for (auto& script : Scripts)
		{
			script->OnCreate();
		}
	}

	void ScriptComponent::UpdateScripts(double deltaTime)
	{
		for (auto& script : Scripts)
		{
			script->OnUpdate(deltaTime);
		}
	}

	void ScriptComponent::DestroyScripts()
	{
		for (auto& script : Scripts)
		{
			script->OnDestroy();
		}
	}

	std::vector<char> ScriptComponent::Serialize()
	{
		std::vector<char> bytes;

		for (int i = 0; i < ScriptClassesNames.size(); i++)
		{
			for (int j = 0; j < ScriptClassesNames[i].size(); j++)
			{
				bytes.push_back(ScriptClassesNames[i][j]);
			}
		}

		bytes.push_back('\0');

		return bytes;
	}

	void ScriptComponent::Deserialize(const std::vector<char>& bytes)
	{
		std::vector<std::string> scriptClassNames(1);

		if (bytes[0] == '\0') // No scripts attached
			return;

		for (int i = 0; i < bytes.size(); i++)
		{
			if (bytes[i] == '\0')
			{
				scriptClassNames.emplace_back("");
			}

			scriptClassNames[scriptClassNames.size() - 1].push_back(bytes[i]);
		}

		scriptClassNames.erase(scriptClassNames.end() - 1); // last index is always empty due to for loop above

		ScriptClassesNames = scriptClassNames;

		for (int i = 0; i < scriptClassNames.size(); i++)
		{
			ScriptInterface* scInterface = (ScriptInterface*)Serializer::CreateRegisteredClass(scriptClassNames[i]);

			VL_CORE_ASSERT(scInterface != nullptr, "Script doesn't inherit from script interface!");

			Scripts.push_back(scInterface);
		}
	}

	ScriptComponent::~ScriptComponent()
	{
		for (auto& script : Scripts)
		{
			delete script;
		}
	}

	ScriptComponent::ScriptComponent(ScriptComponent&& other) noexcept
	{
		Scripts = std::move(other.Scripts);
		ScriptClassesNames = std::move(other.ScriptClassesNames);
	}

	std::vector<char> TransformComponent::Serialize()
	{
		return VulkanHelper::Bytes::ToBytes(this, sizeof(TransformComponent));
	}

	void TransformComponent::Deserialize(const std::vector<char>& bytes)
	{
		TransformComponent comp = VulkanHelper::Bytes::FromBytes<TransformComponent>(bytes);
		memcpy(&Transform, &comp.Transform, sizeof(VulkanHelper::Transform));
	}

	std::vector<char> MeshComponent::Serialize()
	{
		std::vector<char> bytes;

		VulkanHelper::Mesh* mesh = AssetHandle.GetMesh();

		// Get the vulkan buffers
		uint64_t vertexCount = mesh->GetVertexCount();
		uint64_t indexCount = mesh->GetIndexCount();
		VulkanHelper::Buffer* vertexBuffer = mesh->GetVertexBuffer();
		VulkanHelper::Buffer* indexBuffer = mesh->GetIndexBuffer();

		std::vector<char> vertices(vertexCount * sizeof(VulkanHelper::Mesh::Vertex));
		std::vector<char> indices(indexCount * sizeof(uint32_t));

		// Read buffers into vectors
		vertexBuffer->ReadFromBuffer(vertices.data(), vertices.size(), 0);
		if (mesh->HasIndexBuffer()) // skip if empty
			indexBuffer->ReadFromBuffer(indices.data(), indices.size(), 0);

		// Start serializing

		// Serialize Mesh Unique Path
		std::string path = AssetHandle.GetAsset()->GetPath();
		for (int i = 0; i < path.size(); i++)
		{
			bytes.push_back(path[i]);
		}
		bytes.push_back('\0');

		// Serialize the amount of data
		std::vector<char> vertexCountBytes = VulkanHelper::Bytes::ToBytes(&vertexCount, 8);
		std::vector<char> indexCountBytes = VulkanHelper::Bytes::ToBytes(&indexCount, 8);

		bytes.insert(bytes.end(), vertexCountBytes.begin(), vertexCountBytes.end());
		bytes.insert(bytes.end(), indexCountBytes.begin(), indexCountBytes.end()); // don't skip size even when no index buffer

		// Serialize the mesh data
		bytes.insert(bytes.end(), vertices.begin(), vertices.end());

		if (mesh->HasIndexBuffer()) // Skip data if empty
			bytes.insert(bytes.end(), indices.begin(), indices.end());

		return bytes;
	}

	void MeshComponent::Deserialize(const std::vector<char>& bytes)
	{
		uint64_t vertexCount = 0;
		uint64_t indexCount = 0;

		uint64_t currentPos = 0;
		std::string path;
		while (true)
		{
			char ch = bytes[currentPos];
			currentPos++;

			if (ch == '\0')
				break;
			else
				path.push_back(ch);
		}

		// Get the data sizes
		memcpy(&vertexCount, bytes.data() + currentPos, 8);
		currentPos += 8;
		memcpy(&indexCount, bytes.data() + currentPos, 8);
		currentPos += 8;

		// Get the mesh data itself
		std::vector<VulkanHelper::Mesh::Vertex> vertices(vertexCount);
		std::vector<uint32_t> indices(indexCount);

		memcpy(vertices.data(), bytes.data() + currentPos, vertices.size() * sizeof(VulkanHelper::Mesh::Vertex));
		currentPos += vertices.size() * sizeof(VulkanHelper::Mesh::Vertex);
		memcpy(indices.data(), bytes.data() + currentPos, indices.size() * sizeof(uint32_t));
		currentPos += indices.size() * sizeof(uint32_t);

		// Create the mesh
		VulkanHelper::Mesh mesh;
		mesh.Init({ &vertices, &indices });

		// Create the asset
		std::unique_ptr<VulkanHelper::Asset> meshAsset = std::make_unique<VulkanHelper::MeshAsset>(std::move(mesh));
		AssetHandle = AssetManager::AddAsset(path, std::move(meshAsset));
	}

	std::vector<char> NameComponent::Serialize()
	{
		std::vector<char> bytes;
		for (int i = 0; i < Name.size(); i++)
		{
			bytes.push_back(Name[i]);
		}
		bytes.push_back('\0');

		return bytes;
	}

	void NameComponent::Deserialize(const std::vector<char>& bytes)
	{
		std::string name;

		int index = 0;
		while (true)
		{
			char ch = bytes[index];
			index++;

			if (ch == '\0')
				break;
			else
				name.push_back(ch);
		}

		Name = std::move(name);
	}

	std::vector<char> MaterialComponent::Serialize()
	{
		Material* mat = AssetHandle.GetMaterial();

		// Properties
		std::vector<char> bytes;
		bytes.resize(sizeof(MaterialProperties));
		memcpy(bytes.data(), &mat->Properties, sizeof(MaterialProperties));

		// Textures names
		// 1. Albedo
		// 2. Normal
		// 3. Rougness
		// 4. Metallness
		std::vector<std::string> names;
		names.emplace_back(mat->Textures.AlbedoTexture.GetAsset()->GetPath());
		names.emplace_back(mat->Textures.NormalTexture.GetAsset()->GetPath());
		names.emplace_back(mat->Textures.RoughnessTexture.GetAsset()->GetPath());
		names.emplace_back(mat->Textures.MetallnessTexture.GetAsset()->GetPath());

		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < names[i].size(); j++)
			{
				bytes.push_back(names[i][j]);
			}
			bytes.push_back('\0');
		}

		// Material Name
		for (int i = 0; i < mat->MaterialName.size(); i++)
		{
			bytes.push_back(mat->MaterialName[i]);
		}
		bytes.push_back('\0');

		return bytes;
	}

	void MaterialComponent::Deserialize(const std::vector<char>& bytes)
	{
		// Properties
		MaterialProperties props{};
		memcpy(&props, bytes.data(), sizeof(MaterialProperties));

		// Texture Names
		std::vector<std::string> names;

		int currentPos = sizeof(MaterialProperties);
		for (int i = 0; i < 4; i++)
		{
			names.push_back("");
			while (true)
			{
				char ch = bytes[currentPos];
				currentPos += 1;

				if (ch == '\0')
					break;
				else
					names[i].push_back(ch);
			}
		}

		// Material Name
		std::string materialName;
		while (true)
		{
			char ch = bytes[currentPos];
			materialName.push_back(ch);
			currentPos += 1;

			if (ch == '\0')
				break;
		}

		// Making an asset
		std::hash<std::string> hash;
		VulkanHelper::AssetHandle handle = VulkanHelper::AssetHandle(AssetHandle::CreateInfo{ hash(materialName) });

		if (!handle.DoesHandleExist())
		{
			MaterialTextures textures{};

			textures.AlbedoTexture = AssetManager::LoadAsset(names[0]);
			textures.NormalTexture = AssetManager::LoadAsset(names[1]);
			textures.RoughnessTexture = AssetManager::LoadAsset(names[2]);
			textures.MetallnessTexture = AssetManager::LoadAsset(names[3]);

			VulkanHelper::Material mat;
			mat.Properties = std::move(props);
			mat.Textures = std::move(textures);
			mat.MaterialName = materialName;

			std::unique_ptr<Asset> asset = std::make_unique<MaterialAsset>(std::move(mat));
			AssetHandle = AssetManager::AddAsset(materialName, std::move(asset));
		}
		else
		{
			AssetHandle = handle;
		}
	}

	std::vector<char> TonemapperSettingsComponent::Serialize()
	{
		std::vector<char> bytes;

		bytes.resize(sizeof(VulkanHelper::Tonemap::TonemapInfo));

		memcpy(bytes.data(), &Settings, sizeof(VulkanHelper::Tonemap::TonemapInfo));

		return bytes;
	}

	void TonemapperSettingsComponent::Deserialize(const std::vector<char>& bytes)
	{
		memcpy(&Settings, bytes.data(), sizeof(VulkanHelper::Tonemap::TonemapInfo));
	}

	std::vector<char> BloomSettingsComponent::Serialize()
	{
		std::vector<char> bytes;

		bytes.resize(sizeof(VulkanHelper::Bloom::BloomInfo));

		memcpy(bytes.data(), &Settings, sizeof(VulkanHelper::Bloom::BloomInfo));

		return bytes;
	}

	void BloomSettingsComponent::Deserialize(const std::vector<char>& bytes)
	{
		memcpy(&Settings, bytes.data(), sizeof(VulkanHelper::Bloom::BloomInfo));
	}

}