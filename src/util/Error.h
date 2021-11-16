#pragma once

#include <iostream>
#include <sstream>

#include "NamespaceDecl.h"

NAMESPACE_BEGIN(Error)

static void log(const std::string& pri, const std::string& snd = "")
{
    std::cerr << "[" << pri << "]\t" << snd << std::endl;
}

static void exit(const std::string& msg = "")
{
    std::cerr << "[Error exit]\t" << msg << std::endl;
    std::abort();
}

static void impossiblePath()
{
    exit("[Impossible path]\tThis path is impossible to be reached, check the program");
}

static void check(bool cond, const std::string& errMsg = "")
{
    if (!cond)
    {
        std::cerr << "[Check failed]\t" << errMsg << std::endl;
        std::abort();
    }
}

NAMESPACE_END(Error)