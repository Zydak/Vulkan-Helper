#include "pch.h"
#include "Mesh.h"

namespace VulkanHelper
{

	void Mesh::Init(const CreateInfo& createInfo)
	{
		if (m_Initialized)
			Destroy();

		CreateMesh(createInfo);
		m_Initialized = true;
	}

	void Mesh::Init(aiMesh* mesh, const aiScene* scene, const glm::mat4& mat, VkBufferUsageFlags customUsageFlags)
	{
		if (m_Initialized)
			Destroy();

		CreateMesh(mesh, scene, mat, customUsageFlags);
		m_Initialized = true;
	}

	void Mesh::Destroy()
	{
		if (!m_Initialized)
			return;

		m_VertexBuffer.Destroy();
		if (m_HasIndexBuffer)
			m_IndexBuffer.Destroy();

		Reset();
	}

	Mesh::~Mesh()
	{
		Destroy();
	}

	void Mesh::CreateMesh(const CreateInfo& createInfo)
	{
		CreateVertexBuffer(createInfo.Vertices, createInfo.VertexUsageFlags);
		CreateIndexBuffer(createInfo.Indices, createInfo.IndexUsageFlags);
	}

	void Mesh::CreateMesh(aiMesh* mesh, const aiScene* scene, glm::mat4 mat, VkBufferUsageFlags customUsageFlags)
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		// vertices
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			Vertex vertex;
			glm::vec3 vector;

			// positions
			vector.x = mesh->mVertices[i].x;
			vector.y = mesh->mVertices[i].y;
			vector.z = mesh->mVertices[i].z;
			vertex.Position = mat * glm::vec4(vector, 1.0f);

			// normals
			if (mesh->HasNormals())
			{
				vector.x = mesh->mNormals[i].x;
				vector.y = mesh->mNormals[i].y;
				vector.z = mesh->mNormals[i].z;
				vertex.Normal = glm::normalize(glm::vec3(mat * glm::vec4(vector, 0.0f)));
			}

			// texture coordinates
			if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates
			{
				glm::vec2 vec;
				// a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
				// use models where a vertex can have multiple texture coordinates so we always take the first set (0).
				vec.x = mesh->mTextureCoords[0][i].x;
				vec.y = mesh->mTextureCoords[0][i].y;
				vertex.TexCoord = vec;
			}
			else
				vertex.TexCoord = glm::vec2(0.0f, 0.0f);

			vertices.push_back(vertex);
		}

		// indices
		for (unsigned int i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];
			for (unsigned int j = 0; j < face.mNumIndices; j++)
				indices.push_back(face.mIndices[j]);
		}

		CreateVertexBuffer(&vertices);
		CreateIndexBuffer(&indices);
	}

	void Mesh::CreateVertexBuffer(const std::vector<Vertex>* const vertices, VkBufferUsageFlags customUsageFlags)
	{
		m_VertexCount = (uint64_t)vertices->size();
		VkDeviceSize bufferSize = sizeof(Vertex) * m_VertexCount;
		uint32_t vertexSize = sizeof(Vertex);

		Buffer::CreateInfo bufferInfo{};
		bufferInfo.InstanceSize = vertexSize;
		bufferInfo.InstanceCount = m_VertexCount;
		bufferInfo.UsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		Buffer stagingBuffer;
		stagingBuffer.Init(bufferInfo);

		stagingBuffer.Map();
		stagingBuffer.WriteToBuffer((void*)vertices->data());
		stagingBuffer.Flush();

		VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		if (Device::UseRayTracing())
			usageFlags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		
		bufferInfo.UsageFlags = usageFlags | customUsageFlags;
		bufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		m_VertexBuffer.Init(bufferInfo);

		Buffer::CopyBuffer(stagingBuffer.GetBuffer(), m_VertexBuffer.GetBuffer(), bufferSize, 0, 0, Device::GetGraphicsQueue(), 0, Device::GetGraphicsCommandPool());
	}

	void Mesh::CreateIndexBuffer(const std::vector<uint32_t>* const  indices, VkBufferUsageFlags customUsageFlags)
	{
		if (indices == nullptr)
		{
			m_HasIndexBuffer = false;
			return;
		}
		m_IndexCount = (uint32_t)indices->size();
		m_HasIndexBuffer = m_IndexCount > 0;
		if (!m_HasIndexBuffer) { return; }

		VkDeviceSize bufferSize = sizeof(uint32_t) * m_IndexCount;
		uint32_t indexSize = sizeof(uint32_t);

		Buffer::CreateInfo bufferInfo{};
		bufferInfo.InstanceSize = indexSize;
		bufferInfo.InstanceCount = m_IndexCount;
		bufferInfo.UsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		Buffer stagingBuffer;
		stagingBuffer.Init(bufferInfo);

		stagingBuffer.Map();
		stagingBuffer.WriteToBuffer((void*)indices->data());
		stagingBuffer.Flush();

		VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		if (Device::UseRayTracing())
			usageFlags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		
		bufferInfo.InstanceSize = indexSize;
		bufferInfo.InstanceCount = m_IndexCount;
		bufferInfo.UsageFlags = usageFlags | customUsageFlags;
		bufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		m_IndexBuffer.Init(bufferInfo);

		Buffer::CopyBuffer(stagingBuffer.GetBuffer(), m_IndexBuffer.GetBuffer(), bufferSize, 0, 0, Device::GetGraphicsQueue(), 0, Device::GetGraphicsCommandPool());
	}

	void Mesh::Reset()
	{
		m_VertexCount = 0;
		m_HasIndexBuffer = false;
		m_IndexCount = 0;
		m_Initialized = false;
	}

	Mesh::Mesh(Mesh&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_VertexBuffer = std::move(other.m_VertexBuffer);
		m_VertexCount = std::move(other.m_VertexCount);
		m_HasIndexBuffer = std::move(other.m_HasIndexBuffer);
		m_IndexBuffer = std::move(other.m_IndexBuffer);
		m_IndexCount = std::move(other.m_IndexCount);
		m_Initialized = std::move(other.m_Initialized);

		other.Reset();
	}

	Mesh::Mesh(const CreateInfo& createInfo)
	{
		Init(createInfo);
	}

	Mesh::Mesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& mat /*= glm::mat4(1.0f)*/, VkBufferUsageFlags customUsageFlags /*= 0*/)
	{
		Init(mesh, scene, mat, customUsageFlags);
	}

	Mesh& Mesh::operator=(Mesh&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_VertexBuffer = std::move(other.m_VertexBuffer);
		m_VertexCount = std::move(other.m_VertexCount);
		m_HasIndexBuffer = std::move(other.m_HasIndexBuffer);
		m_IndexBuffer = std::move(other.m_IndexBuffer);
		m_IndexCount = std::move(other.m_IndexCount);
		m_Initialized = std::move(other.m_Initialized);

		other.Reset();

		return *this;
	}

	/**
		@brief Binds vertex and index buffers
	*/
	void Mesh::Bind(VkCommandBuffer commandBuffer)
	{
		VkBuffer buffers[] = { m_VertexBuffer.GetBuffer() };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

		if (m_HasIndexBuffer) 
		{ 
			vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer.GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
		}
	}

	void Mesh::Draw(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance)
	{
		if (m_HasIndexBuffer)
		{ 
			vkCmdDrawIndexed(commandBuffer, (uint32_t)m_IndexCount, instanceCount, 0, 0, firstInstance); 
		}
		else 
		{ 
			vkCmdDraw(commandBuffer, (uint32_t)m_VertexCount, instanceCount, 0, firstInstance); 
		}
	}

	/**
	 * @brief Specifies how many vertex buffers we wish to bind to our pipeline. In this case there is only one with all data packed inside it
	*/
	std::vector<VkVertexInputBindingDescription> Mesh::Vertex::GetBindingDescriptions()
	{
		std::vector<VkVertexInputBindingDescription> bindingDescription(1);
		bindingDescription[0].binding = 0;
		bindingDescription[0].stride = sizeof(Vertex);
		bindingDescription[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	/**
	 * @brief Specifies layout of data inside vertex buffer
	*/
	std::vector<VkVertexInputAttributeDescription> Mesh::Vertex::GetAttributeDescriptions()
	{
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
		attributeDescriptions.reserve(5);
		attributeDescriptions.emplace_back(VkVertexInputAttributeDescription{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Position) });
		attributeDescriptions.emplace_back(VkVertexInputAttributeDescription{ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Normal) });
		attributeDescriptions.emplace_back(VkVertexInputAttributeDescription{ 2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, TexCoord) });

		return attributeDescriptions;
	}

	void Mesh::UpdateVertexBuffer(const std::vector<Vertex>& vertices, int offset, VkCommandBuffer cmd)
	{
		vkCmdUpdateBuffer(cmd, m_VertexBuffer.GetBuffer(), offset, sizeof(vertices[0]) * vertices.size(), vertices.data());
	}

	void Mesh::UpdateIndexBuffer(const std::vector<uint32_t>& indices, int offset, VkCommandBuffer cmd /*= 0*/)
	{
		vkCmdUpdateBuffer(cmd, m_IndexBuffer.GetBuffer(), offset, sizeof(indices[0]) * indices.size(), indices.data());
	}

}