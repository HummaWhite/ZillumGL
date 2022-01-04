#pragma once

#include <optional>
#include <stack>

#include "../util/NamespaceDecl.h"
#include "../util/File.h"
#include "Texture.h"
#include "Model.h"

class Resource
{
public:
	static ImagePtr getImageByIndex(int index);
	static ImagePtr getImageByPath(const File::path& path);
	static int addImage(const File::path& path, ImageDataType type);
	static std::vector<ImagePtr>& getAllImages() { return imagePool; }

	static ModelInstancePtr createNewModelInstance(const File::path& path);
	static ModelInstancePtr getModelInstanceByPath(const File::path& path);
	static ModelInstancePtr openModelInstance(const File::path& path,
		const glm::vec3& pos, const glm::vec3& scale = glm::vec3(1.0f), const glm::vec3& rotation = glm::vec3(0.0f));

	static void clear();

private:
	static MeshInstancePtr createNewMeshInstance(aiMesh* mesh, const aiScene* scene, const File::path& instancePath);

private:
	static std::vector<ImagePtr> imagePool;
	static std::map<File::path, int> mapPathToImageIndex;

	static std::vector<MeshDataPtr> meshDataPool;

	static std::map<File::path, ModelInstancePtr> mapPathToModelInstance;
};