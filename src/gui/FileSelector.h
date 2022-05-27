#pragma once

#include <iostream>
#include <optional>
#include <vector>

#include "../thirdparty/imgui.hpp"
#include "../util/NamespaceDecl.h"
#include "../util/File.h"

class FileSelector
{
public:
	FileSelector() : mCurrent("./") {}
	FileSelector(const File::path& path) : mCurrent(path) {}

	std::optional<File::path> show();

	bool& isOpen() { return mOpen; }

private:
	File::directory_entry mCurrent;
	bool mOpen = false;
};