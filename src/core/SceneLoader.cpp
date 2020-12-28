#include "SceneLoader.h"

bool SceneLoader::loadScene(std::vector<std::shared_ptr<Model>>& models, std::vector<MaterialPBR>& materials, std::vector<std::shared_ptr<Light>>& lights, const char* filePath)
{
	std::fstream file(filePath);
	std::cout << "Loading Scene: " << filePath << std::endl;

	if (!file.is_open())
	{
		std::cout << "Error: Scene file not found" << std::endl;
		return false;
	}

	std::string section;
	while (std::getline(file, section))
	{
		if (section == "#Objects") break;
	}

	int objectsCount;
	file >> objectsCount;
	for (int i = 0; i < objectsCount; i++)
	{
		std::cout << "  Object #" << i << std::endl;
		std::string modelPath, tmp;
		glm::vec3 modelPos(0.0f);
		glm::vec3 modelScale(1.0f);
		glm::vec3 modelRot(0.0f);
		MaterialPBR modelMaterial(Material::defaultMaterialPBR);
		file >> modelPath;
		file >> tmp;
		if (tmp == "P")
		{
			file >> modelPos.x >> modelPos.y >> modelPos.z;
		}
		file >> tmp;
		if (tmp == "S")
		{
			file >> modelScale.x >> modelScale.y >> modelScale.z;
		}
		file >> tmp;
		if (tmp == "R")
		{
			file >> modelRot.x >> modelRot.y >> modelRot.z;
		}
		file >> tmp;
		if (tmp == "M")
		{
			file >> modelMaterial.albedo.r >> modelMaterial.albedo.g >> modelMaterial.albedo.b;
			file >> modelMaterial.metallic >> modelMaterial.roughness >> modelMaterial.ao;
		}
		std::shared_ptr<Model> model = std::make_shared<Model>(modelPath.c_str(), modelPos);
		model->setScale(modelScale);
		model->setRotation(modelRot);
		models.push_back(model);
		materials.push_back(modelMaterial);
	}

	while (std::getline(file, section))
	{
		if (section == "#Lights") break;
	}

	int lightCount;
	file >> lightCount;
	for (int i = 0; i < lightCount; i++)
	{
		std::cout << "  Light #" << i << std::endl;
		std::string tmp;
		glm::vec3 lightPos(0.0f), lightColor(1.0f);
		glm::vec3 lightDir(0.0f, 0.0f, -1.0f);
		float cutoff(180.0f), outerCutoff(180.0f);
		float size(0.3f), strength(4.0f);
		file >> tmp;
		if (tmp == "P")
		{
			file >> lightPos.x >> lightPos.y >> lightPos.z;
		}
		file >> tmp;
		if (tmp == "C")
		{
			file >> lightColor.x >> lightColor.y >> lightColor.z;
		}
		file >> tmp;
		if (tmp == "D")
		{
			file >> lightDir.x >> lightDir.y >> lightDir.z;
		}
		file >> tmp;
		if (tmp == "R")
		{
			file >> cutoff >> outerCutoff;
		}
		file >> tmp;
		if (tmp == "S")
		{
			file >> size >> strength;
		}
		std::shared_ptr<Light> light = std::make_shared<Light>(lightPos, lightColor, lightDir, cutoff, outerCutoff);
		light->size = size, light->strength = strength;
		lights.push_back(light);
	}

	file.close();
	std::cout << "Loading Scene Done." << std::endl;
	return true;
}

bool SceneLoader::saveScene(std::vector<std::shared_ptr<Model>>& models, std::vector<MaterialPBR>& materials, std::vector<std::shared_ptr<Light>>& lights, const char* filePath)
{
	std::ofstream saveFile(filePath);

	if (!saveFile.is_open())
	{
		return false;
	}

	saveFile << "#Objects" << std::endl;
	saveFile << models.size() << std::endl;
	for (int i = 0; i < models.size(); i++)
	{
		std::shared_ptr<Model> mod = models[i];
		saveFile << mod->name() << std::endl;
		saveFile << "P " << std::fixed << mod->pos().x << " " << mod->pos().y << " " << mod->pos().z << std::endl;
		saveFile << "S " << std::fixed << mod->scale().x << " " << mod->scale().y << " " << mod->scale().z << std::endl;
		saveFile << "R " << std::fixed << mod->rotation().x << " " << mod->rotation().y << " " << mod->rotation().z << std::endl;

		MaterialPBR mat = materials[i];
		saveFile << "M " << std::fixed << mat.albedo.r << " " << mat.albedo.g << " " << mat.albedo.b << " ";
		saveFile << mat.metallic << " " << mat.roughness << " " << mat.ao << std::endl;
		saveFile << std::endl;
	}

	saveFile << std::endl << "#Lights" << std::endl;
	saveFile << lights.size() << std::endl;
	for (auto i : lights)
	{
		saveFile << "P " << std::fixed << i->pos.x << " " << i->pos.y << " " << i->pos.z << std::endl;
		saveFile << "C " << std::fixed << i->color.r << " " << i->color.g << " " << i->color.b << std::endl;
		saveFile << "D " << std::fixed << i->dir.x << " " << i->dir.y << " " << i->dir.z << std::endl;
		saveFile << "R " << std::fixed << i->cutoff << " " << i->outerCutoff << std::endl;
		saveFile << "S " << std::fixed << i->size << " " << i->strength << std::endl;
		saveFile << std::endl;
	}

	saveFile.close();
	return true;
}
