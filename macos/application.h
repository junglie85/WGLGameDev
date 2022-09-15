#pragma once

#include <OpenGL/gl3.h>
#include <iostream>

class Application {
private:
    GLuint program;
    GLuint vao;

public:
    Application();
    void update();
    ~Application();
};
