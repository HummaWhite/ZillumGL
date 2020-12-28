#include "Model.h"

glm::mat4 Model::constRot = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

Model::Model() :
	m_Pos(glm::vec3(0.0f)),
	m_RotMatrix(glm::mat4(1.0f)),
	m_Scale(glm::vec3(1.0f)),
	m_LoadedFromFile(false)
{
}

Model::Model(const char* filePath, glm::vec3 pos, float size) :
	m_Pos(pos),
	m_Scale(size),
	m_RotMatrix(glm::mat4(1.0f)),
	m_LoadedFromFile(true)
{
	loadModel(filePath);
}

Model::Model(Shape& shape, glm::vec3 pos, float size) :
	m_Pos(pos),
	m_Scale(size),
	m_RotMatrix(glm::mat4(1.0f)),
	m_LoadedFromFile(false)
{
	loadShape(shape);
}

Model::~Model()
{
	for (auto i : m_Meshes)
	{
		if (i != nullptr)
		{
			delete i;
		}
	}
}

bool Model::loadModel(const char* filePath)
{
	m_LoadedFromFile = true;
	Assimp::Importer importer;
	GLuint option = aiProcess_Triangulate
		| aiProcess_FlipUVs
		| aiProcess_GenSmoothNormals
		| aiProcess_GenUVCoords
		| aiProcess_FixInfacingNormals
		| aiProcess_FindInstances
		| aiProcess_JoinIdenticalVertices
		| aiProcess_OptimizeMeshes
		| aiProcess_GenUVCoords
		//| aiProcess_ForceGenNormals
		//| aiProcess_FindDegenerates
		;
	const aiScene* scene = importer.ReadFile(filePath, option);

	std::cout << "Loading Model: " << filePath << " ..." << std::endl;

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		std::cout << "Error: Assimp::" << importer.GetErrorString() << std::endl;
		return false;
	}

	processNode(scene->mRootNode, scene);

	std::cout << "Done" << std::endl;
	m_Name = std::string(filePath);
	return true;
}

void Model::draw(Shader& shader)
{
	shader.setMat4("model", this->modelMatrix());
	for (auto i : m_Meshes)
	{
		i->draw(shader);
	}
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
	model = glm::rotate(model, glm::radians(m_Rotation.x), glm::vec3(0.0f, 0.0f, 1.0f));
	model = glm::rotate(model, glm::radians(m_Rotation.y), glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::rotate(model, glm::radians(m_Rotation.z), glm::vec3(0.0f, 1.0f, 0.0f));
	model = glm::scale(model, m_Scale);
	if (m_LoadedFromFile) model = model * constRot;
	return model;
}

void Model::loadShape(Shape& shape)
{
	Mesh* mesh = new Mesh();
	mesh->loadShape(shape);
	m_Meshes.push_back(mesh);
	m_Name = "Loaded From Shape";
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

Mesh* Model::processMesh(aiMesh* mesh, const aiScene* scene)
{
	std::cout << "	Processing Mesh... ";
	std::vector<GLuint> indices;
	for (int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (int j = 0; j < face.mNumIndices; j++)
			indices.push_back(face.mIndices[j]);
	}

	std::vector<Mesh::POS3_UV2_NORM3> data;
	int validVertexCount = 0;
	for (int i = 0; i < mesh->mNumVertices; i++)
	{
		Mesh::POS3_UV2_NORM3 vertex;

		vertex.pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
		vertex.uv = mesh->mTextureCoords[0] ?
			glm::vec2{ mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y } :
			glm::vec2{ 0, 0 };
		vertex.norm = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };

		data.push_back(vertex);
	}

	Mesh* newMesh = new Mesh;
	newMesh->loadMesh(&data[0], mesh->mNumVertices, indices, LAYOUT_POS3_UV2_NORM3);

	std::cout << "Done" << std::endl;

	return newMesh;
}
