#include "Resource.h"

namespace Resource
{
	Image::Image(const std::string& path)
	{
		int channels;
		uint8_t* tmp = stbi_load(path.c_str(), &width, &height, &channels, 3);
		if (tmp == nullptr)
		{
			std::cout << "[Image]\tFailed to load " << path << "\n";
			return;
		}
		size_t size = width * height * 3;
		data = new uint8_t[size];
		memcpy(data, tmp, size);
		stbi_image_free(tmp);
	}

	Image::~Image()
	{
		if (data != nullptr)
			delete[] data;
	}

	int addImage(const std::string& path)
	{
		auto res = imageMap.find(path);
		if (res != imageMap.end())
			return res->second;
		imageMap[path] = images.size();
		images.push_back(std::make_shared<Image>(path));
		return images.size() - 1;
	}

	std::optional<int> getImageIndex(const std::string& path)
	{
		auto res = imageMap.find(path);
		if (res == imageMap.end())
			return std::nullopt;
		return res->second;
	}

	ImagePtr getImage(int index)
	{
		return images[index];
	}

	std::pair<TexturePtr, std::vector<glm::vec2>> createTextureBatch()
	{
		auto tex = std::make_shared<Texture>();
		tex->create(GL_TEXTURE_2D_ARRAY);

		int maxWidth = 0;
		int maxHeight = 0;

		for (const auto& i : images)
		{
			maxWidth = std::max(maxWidth, i->width);
			maxHeight = std::max(maxHeight, i->height);
		}

		glTextureImage3DEXT(tex->ID(), GL_TEXTURE_2D_ARRAY, 0, GL_SRGB,
			maxWidth, maxHeight, images.size(), 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

		for (int i = 0; i < images.size(); i++)
		{
			const auto& img = images[i];
			glTextureSubImage3D(tex->ID(), 0, 0, 0, i,
				img->width, img->height, 1, GL_RGB, GL_UNSIGNED_BYTE, img->data);
		}

		glTextureParameteri(tex->ID(), GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(tex->ID(), GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTextureParameteri(tex->ID(), GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(tex->ID(), GL_TEXTURE_WRAP_T, GL_REPEAT);

		std::vector<glm::vec2> uvScale(images.size());
		for (int i = 0; i < images.size(); i++)
		{
			const auto& img = images[i];
			uvScale[i] = glm::vec2(img->width, img->height) / glm::vec2(maxWidth, maxHeight);
		}
		return { tex, uvScale };
	}
}