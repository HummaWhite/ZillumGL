#include "Model.h"

#include <filesystem>

glm::mat4 Model::constRot = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

Model::Model(const char* filePath, const glm::vec3& pos, const glm::vec3& scale, const glm::vec3& rotation) :
	m_Pos(pos),
	m_Scale(scale),
	m_Rotation(rotation),
	m_RotMatrix(glm::mat4(1.0f)),
	m_LoadedFromFile(true)
{
	loadModel(filePath);
}

Model::~Model()
{
}

bool Model::loadModel(const char* filePath)
{
	m_Name = std::string(filePath);
	m_LoadedFromFile = true;
	Assimp::Importer importer;
	uint32_t option = aiProcess_Triangulate
		| aiProcess_FlipUVs
		| aiProcess_GenSmoothNormals
		| aiProcess_GenUVCoords
		| aiProcess_FixInfacingNormals
		| aiProcess_FindInstances
		| aiProcess_JoinIdenticalVertices
		| aiProcess_OptimizeMeshes
		//| aiProcess_ForceGenNormals
		//| aiProcess_FindDegenerates
		;
	const aiScene* scene = importer.ReadFile(filePath, option);

	std::cout << "[Model]\t\tLoading [" << filePath << "] ...\n";

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		std::cout << "[Error]\t\tAssimp: " << importer.GetErrorString() << std::endl;
		return false;
	}

	if (scene->mNumMaterials > 0)
	{
		for (int i = 0; i < scene->mNumMaterials; i++)
		{
			aiMaterial* mat = scene->mMaterials[i];
			aiColor3D albedo;
			mat->Get(AI_MATKEY_COLOR_DIFFUSE, albedo);
			m_Materials.push_back(Material(*(glm::vec3*)&albedo));
			//aiString texPath;
			//mat->Get(AI_MATKEY_TEXTURE_DIFFUSE, tex);
			//mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
			//if (texPath.length != 0) std::cout << texPath.C_Str() << "\n";
		}
	}

	processNode(scene->mRootNode, scene);

	std::cout << "  [" << m_Meshes.size() << " mesh(es)]" << std::endl;
	return true;
}

void Model::setPos(glm::vec3 pos)
{
	m_Pos = pos;
}

void Model::setPos(float x, float y, float z)
{
	setPos(glm::vec3(x, y, z));
}

void Model::move(glm::vec3 vec)
{
	m_Pos += vec;
}

void Model::rotateObjectSpace(float angle, glm::vec3 axis)
{
	m_RotMatrix = glm::rotate(m_RotMatrix, glm::radians(angle), axis);
}

void Model::rotateWorldSpace(float angle, glm::vec3 axis)
{
	m_RotMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(angle), axis) * m_RotMatrix;
}

void Model::setScale(glm::vec3 scale)
{
	m_Scale = scale;
}

void Model::setScale(float xScale, float yScale, float zScale)
{
	m_Scale = glm::vec3(xScale, yScale, zScale);
}

void Model::setRotation(glm::vec3 angle)
{
	m_Rotation = angle;
}

void Model::setRotation(float yaw, float pitch, float roll)
{
	m_Rotation = glm::vec3(yaw, pitch, roll);
}

void Model::setSize(float size)
{
	m_Scale = glm::vec3(size);
}

glm::mat4 Model::modelMatrix() const
{
	glm::mat4 model(1.0f);
	model = glm::translate(model, m_Pos);
	model = glm::scale(model, m_Scale);
	if (m_LoadedFromFile) model = model * constRot;
	model = glm::rotate(model, glm::radians(m_Rotation.x), glm::vec3(0.0f, 0.0f, 1.0f));
	model = glm::rotate(model, glm::radians(m_Rotation.y), glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::rotate(model, glm::radians(m_Rotation.z), glm::vec3(0.0f, 1.0f, 0.0f));
	return model;
}

void Model::processNode(aiNode* node, const aiScene* scene)
{
	for (int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		m_Meshes.push_back(processMesh(mesh, scene));
	}

	for (int i = 0; i < node->mNumChildren; i++)
	{
		processNode(node->mChildren[i], scene);
	}
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene)
{
	std::vector<glm::vec3> vertices(mesh->mNumVertices);
	std::vector<glm::vec3> normals(mesh->mNumVertices);
	std::vector<glm::vec2> texCoords(mesh->mNumVertices);
	std::vector<uint32_t> indices;

	for (int i = 0; i < mesh->mNumFaces; i++)
	{
		auto face = mesh->mFaces[i];
		for (int j = 0; j < face.mNumIndices; j++)
			indices.push_back(face.mIndices[j]);
	}

	memcpy(vertices.data(), mesh->mVertices, mesh->mNumVertices * sizeof(glm::vec3));
	memcpy(normals.data(), mesh->mNormals, mesh->mNumVertices * sizeof(glm::vec3));
	if (mesh->mTextureCoords[0] != nullptr)
	{
		for (int i = 0; i < mesh->mNumVertices; i++)
		{
			texCoords[i].x = mesh->mTextureCoords[0][i].x;
			texCoords[i].y = mesh->mTextureCoords[0][i].y;
			//texCoords[i] = glm::fract(texCoords[i]);
		}
	}
	else
	{
		std::cout << "\t\t[Invalid TexCoords]\n";
		memset(texCoords.data(), 0, mesh->mNumVertices * sizeof(glm::vec2));
	}

	auto model = modelMatrix();
	auto modelInv = glm::mat3(glm::transpose(glm::inverse(model)));

	Mesh m;
	m.vertices.resize(vertices.size());
	m.normals.resize(normals.size());
	m.texCoords = texCoords;
	m.indices = indices;

	std::transform(vertices.begin(), vertices.end(), m.vertices.begin(),
		[model](const glm::vec3& v)
		{
			return glm::vec3(model * glm::vec4(v, 1.0f));
		}
	);
	std::transform(normals.begin(), normals.end(), m.normals.begin(),
		[modelInv](const glm::vec3& n)
		{
			return glm::normalize(modelInv * n);
		}
	);

	std::cout << "\t[Mesh]\tnVertices = " << vertices.size() << ", nFaces = " <<
		indices.size() / 3 << "\n";

	auto makeUint32 = [](uint16_t high, uint16_t low) -> uint32_t
	{
		return static_cast<uint32_t>(high) << 16 | low;
	};

	int texIndex = -1;
	if (mesh->mMaterialIndex >= 0)
	{
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
		{
			aiString str;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &str);
			std::filesystem::path p(str.C_Str());
			std::string path = p.is_absolute() ?
				p.generic_string() : m_Name + "/../" + p.generic_string();
			std::cout << "\t\t" << p.generic_string() << "\n";
			std::cout << "\t\t" << path << "\n";
			texIndex = Resource::addImage(path);
		}
	}
	m.matTexIndex = texIndex << 16 | mesh->mMaterialIndex;
	std::cout << "\t\t" << texIndex << " " << mesh->mMaterialIndex << "\n";
	return m;
}
