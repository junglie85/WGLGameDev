#include "App.h"

Application::Application() {
  static const char *vs_source[] = {
      "#version 410 core                                                 \n"
      "                                                                  \n"
      "void main(void)                                                   \n"
      "{                                                                 \n"
      "    const vec4 vertices[] = vec4[](vec4( 0.25, -0.25, 0.5, 1.0),  \n"
      "                                   vec4(-0.25, -0.25, 0.5, 1.0),  \n"
      "                                   vec4( 0.25,  0.25, 0.5, 1.0)); \n"
      "                                                                  \n"
      "    gl_Position = vertices[gl_VertexID];                          \n"
      "}                                                                 \n"};

  static const char *fs_source[] = {
      "#version 410 core                                                 \n"
      "                                                                  \n"
      "out vec4 color;                                                   \n"
      "                                                                  \n"
      "void main(void)                                                   \n"
      "{                                                                 \n"
      "    color = vec4(0.0, 0.8, 1.0, 1.0);                             \n"
      "}                                                                 \n"};

  program = glCreateProgram();
  GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fs, 1, fs_source, NULL);
  glCompileShader(fs);

  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vs, 1, vs_source, NULL);
  glCompileShader(vs);

  glAttachShader(program, vs);
  glAttachShader(program, fs);

  glLinkProgram(program);

  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
}

void Application::update() {
  static const GLfloat green[] = {0.0f, 0.25f, 0.0f, 1.0f};
  glClearBufferfv(GL_COLOR, 0, green);

  glUseProgram(program);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}

Application::~Application() {
  glDeleteVertexArrays(1, &vao);
  glDeleteProgram(program);
}
