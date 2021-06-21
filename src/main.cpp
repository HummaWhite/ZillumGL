#include "RayTest.h"

int main()
{
    RayTest engine;
    Inputs::bindEngine(&engine);
    Inputs::setup();
    return engine.run();
}
