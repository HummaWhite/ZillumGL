#include "RayTest.h"

int main()
{
    RayTest engine(2000, false);
    Inputs::bindEngine(&engine);
    Inputs::setup();
    return engine.run();
}
