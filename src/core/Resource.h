#pragma once

#include <iostream>
#include <optional>
#include <memory>
#include <vector>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Texture.h"

namespace Resource
{
	struct Image
	{
		Image(const std::string& path);
		~Image();

		int width, height;
		uint8_t* data = nullptr;
	};

	using ImagePtr = std::shared_ptr<Image>;

	static std::vector<ImagePtr> images;
	static std::map<std::string, int32_t> imageMap;

	int addImage(const std::string& path);
	std::optional<int> getImageIndex(const std::string& path);
	ImagePtr getImage(int index);

	std::pair<TexturePtr, std::vector<glm::vec2>> createTextureBatch();
}