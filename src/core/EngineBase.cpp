#include "EngineBase.h"

EngineBase::EngineBase() :
    m_Window(nullptr),
    m_ShouldTerminate(false),
    m_ErrorOccured(false)
{
    Error::log("Init", "loading setup.txt");
    std::fstream setupFile("setup.txt");
    Error::check(setupFile.is_open(), "[Init]\tunable to open setup file");

    bool border;
    setupFile >> m_WindowWidth >> m_WindowHeight >> border;
    this->setupGL(m_WindowWidth, m_WindowHeight, border);
}

EngineBase::~EngineBase()
{
}

int EngineBase::run()
{
    this->init();

    while (!this->shouldTerminate() && !glfwWindowShouldClose(this->m_Window))
    {
        this->renderLoop();
    }

    this->terminate();

    if (glfwWindowShouldClose(this->m_Window)) glfwTerminate();

    return this->m_ErrorOccured ? -1 : 0;
}

void EngineBase::setupGL(int width, int height, bool border)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DECORATED, border);

    int nMonitors;
    auto monitors = glfwGetMonitors(&nMonitors);

    Error::log("Physical monitors", "found " + std::to_string(nMonitors));

    m_Window = glfwCreateWindow(width, height, "Zillum", nullptr, nullptr);
    Error::check(m_Window != nullptr, "[GLFW failed to create window]");
    glfwMakeContextCurrent(m_Window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        Error::exit("failed to init GLAD");

    glfwSetInputMode(this->window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void EngineBase::swapBuffers()
{
    glfwSwapBuffers(this->m_Window);
}

void EngineBase::display()
{
    glfwPollEvents();
}

void EngineBase::resizeWindow(int width, int height)
{
    glfwSetWindowSize(this->m_Window, width, height);
    this->m_WindowWidth = width;
    this->m_WindowHeight = height;
}
