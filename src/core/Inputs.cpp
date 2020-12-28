#include "Inputs.h"

namespace Inputs
{
    EngineBase* bindingEngine = nullptr;

    void windowSizeCallback(GLFWwindow* window, int width, int height)
    {
        if (bindingEngine == nullptr) return;
        bindingEngine->processResize(width, height);
    }

    void cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
    {
        if (bindingEngine == nullptr) return;
        bindingEngine->processCursor(xpos, ypos);
    }

    void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
    {
        if (bindingEngine == nullptr) return;
        bindingEngine->processScroll(xoffset, yoffset);
    }

    void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mode)
    {
        if (bindingEngine == nullptr) return;
        bindingEngine->processKey(key, scancode, action, mode);
    }

    void bindEngine(EngineBase* engine)
    {
        bindingEngine = engine;
    }

    void setup()
    {
        if (bindingEngine == nullptr)
        {
            std::cout << "Error: No binding engine for Input" << std::endl;
            return;
        }
        glfwSetWindowSizeCallback(bindingEngine->window(), windowSizeCallback);
        glfwSetCursorPosCallback(bindingEngine->window(), cursorPosCallback);
        glfwSetScrollCallback(bindingEngine->window(), scrollCallback);
        //glfwSetKeyCallback(bindingEngine->window(), keyCallback);
    }
}
