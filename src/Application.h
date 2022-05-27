#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "core/Integrator.h"
#include "util/File.h"
#include "util/Error.h"
#include "util/NamespaceDecl.h"

NAMESPACE_BEGIN(Application)

void init(const std::string& title, const File::path& scenePath);
int run();
void reset(ResetLevel level = ResetLevel::ResetUniform);

NAMESPACE_END(Application)