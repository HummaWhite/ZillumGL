#include "Resource.h"
#include "../util/Error.h"

std::vector<ImagePtr> Resource::imagePool;
std::map<File::path, int> Resource::mapPathToImageIndex;

std::vector<MeshDataPtr> Resource::meshDataPool;
std::map<File::path, ModelInstancePtr> Resource::mapPathToModelInstance;

ImagePtr Resource::getImageByIndex(int index)
{
	Error::check(index < imagePool.size(), "Image index out of bound");
	return imagePool[index];
}

ImagePtr Resource::getImageByPath(const File::path& path)
{
	auto res = mapPathToImageIndex.find(path);
	if (res == mapPathToImageIndex.end())
		return nullptr;
	return getImageByIndex(res->second);
}

int Resource::addImage(const File::path& path, ImageDataType type)
{
	auto res = mapPathToImageIndex.find(path);
	if (res != mapPathToImageIndex.end())
		return res->second;
	auto img = Image::createFromFile(path, type);
	if (!img)
		return -1;
	mapPathToImageIndex[path] = imagePool.size();
	imagePool.push_back(img);
	return imagePool.size() - 1;
}

ModelInstancePtr Resource::createNewModelInstance(const File::path& path)
{
	auto model = std::make_shared<ModelInstance>();
	auto pathStr = path.generic_string();

	model->mPath = pathStr;
	Assimp::Importer importer;
	uint32_t option = aiProcess_Triangulate
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
	auto scene = importer.ReadFile(pathStr.c_str(), option);

	Error::log("ModelInstance", "Loading: " + pathStr + " ...");
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		Error::log("Assimp", std::string(importer.GetErrorString()));
		return nullptr;
	}

	std::stack<aiNode*> stack;
	stack.push(scene->mRootNode);
	while (!stack.empty())
	{
		auto node = stack.top();
		stack.pop();
		for (int i = 0; i < node->mNumMeshes; i++)
		{
			auto mesh = scene->mMeshes[node->mMeshes[i]];
			model->mMeshInstances.push_back(
				createNewMeshInstance(mesh, scene, path.parent_path()));
		}
		for (int i = 0; i < node->mNumChildren; i++)
			stack.push(node->mChildren[i]);
	}

	for (int i = 0; i < scene->mNumMaterials; i++)
	{
		aiMaterial* mat = scene->mMaterials[i];
		aiColor3D albedo;
		mat->Get(AI_MATKEY_COLOR_DIFFUSE, albedo);
		model->mMaterials.push_back(Material(*(glm::vec3*) & albedo));
	}
	Error::line("\t[" + std::to_string(scene->mNumMaterials) + " material(s)]");
	Error::line("\t[" + std::to_string(model->mMeshInstances.size()) + " mesh(es)]");
	return model;
}

ModelInstancePtr Resource::getModelInstanceByPath(const File::path& path)
{
	auto res = mapPathToModelInstance.find(path);
	if (res == mapPathToModelInstance.end())
		return nullptr;
	return res->second;
}

ModelInstancePtr Resource::openModelInstance(const File::path& path,
	const glm::vec3& pos, const glm::vec3& scale, const glm::vec3& rotation)
{
	auto raw = getModelInstanceByPath(path);
	if (raw == nullptr)
		raw = createNewModelInstance(path);
	auto newCopy = raw->copy();
	newCopy->setPos(pos);
	newCopy->setScale(scale);
	newCopy->setRotation(rotation);
	return newCopy;
}

MeshInstancePtr Resource::createNewMeshInstance(aiMesh* mesh, const aiScene* scene, const File::path& instancePath)
{
	Error::line("\t[Mesh]\tnVertices = " + std::to_string(mesh->mNumVertices) +
		", nFaces = " + std::to_string(mesh->mNumFaces));

	auto meshInstance = MeshInstance::create();
	auto meshData = MeshData::create();

	for (int i = 0; i < mesh->mNumVertices; i++)
	{
		meshData->positions.push_back({ mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z });
		meshData->texcoords.push_back(mesh->mTextureCoords[0] ?
			glm::vec2{ mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y } :
			glm::vec2{ 0, 0 });
		meshData->normals.push_back({ mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z });
	}

	for (int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (int j = 0; j < face.mNumIndices; j++)
			meshData->indices.push_back(face.mIndices[j]);
	}

	if (scene->mNumMaterials > 0)
		meshInstance->matIndex = mesh->mMaterialIndex;

	if (mesh->mMaterialIndex >= 0)
	{
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
		{
			aiString str;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &str);
			std::filesystem::path path(str.C_Str());
			if (!path.is_absolute())
				path = instancePath / path;
			Error::line("\t\t[Albedo texture] " + path.generic_string());
			meshInstance->texIndex = Resource::addImage(path, ImageDataType::Int8);
		}
	}
	//meshData->createGLContext();
	meshInstance->meshData = meshData;
	return meshInstance;
}
