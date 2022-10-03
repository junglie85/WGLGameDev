/* Based on https://gist.github.com/nickrolfe/1127313ed1dbf80254b614a721b3ee9c */
/* and https://riptutorial.com/opengl/example/5305/manual-opengl-setup-on-windows */

#include <windows.h>

#include <GL/glcorearb.h>
#include <GL/wglext.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB;
PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLBINDTEXTUREPROC glBindTexture;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
PFNGLBUFFERDATAPROC glBufferData;
PFNGLCLEARPROC glClear;
PFNGLCLEARCOLORPROC glClearColor;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLCREATESHADERPROC glCreateShader;
PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback;
PFNGLDELETEBUFFERSPROC glDeleteBuffers;
PFNGLDELETEPROGRAMPROC glDeleteProgram;
PFNGLDELETESHADERPROC glDeleteShader;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
PFNGLDRAWARRAYSPROC glDrawArrays;
PFNGLDRAWELEMENTSPROC glDrawElements;
PFNGLENABLEPROC glEnable;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
PFNGLGENERATEMIPMAPPROC glGenerateMipmap;
PFNGLGENBUFFERSPROC glGenBuffers;
PFNGLGENTEXTURESPROC glGenTextures;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
PFNGLGETPROGRAMIVPROC glGetProgramiv;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
PFNGLGETSHADERIVPROC glGetShaderiv;
PFNGLGETSTRINGPROC glGetString;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLTEXIMAGE2DPROC glTexImage2D;
PFNGLTEXPARAMETERIPROC glTexParameteri;
PFNGLUNIFORM1FPROC glUniform1f;
PFNGLUNIFORM1IPROC glUniform1i;
PFNGLUSEPROGRAMPROC glUseProgram;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
PFNGLVIEWPORTPROC glViewport;

static void fatal_error(const char* msg)
{
    MessageBox(NULL, msg, "Error", MB_OK | MB_ICONEXCLAMATION);
    exit(EXIT_FAILURE);
}

static void non_fatal_error(const char* msg) { OutputDebugString(TEXT(msg)); }

typedef struct Shader {
    GLuint id;
} Shader;

static GLuint load_shader(GLenum type, const char* shader_src)
{
    GLuint shader = glCreateShader(type);

    if (shader == 0) { return 0; }
    glShaderSource(shader, 1, &shader_src, NULL);
    glCompileShader(shader);

    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint info_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1) {
            char* info_log = (char*)malloc(sizeof(char) * info_len);
            glGetShaderInfoLog(shader, info_len, NULL, info_log);
            fprintf(stderr, "Error compiling this shader:\n%s\n", info_log);
            free(info_log);
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void shader_create(const char* v, const char* f, Shader* shader)
{
    GLuint program, vertex_shader, fragment_shader;

    vertex_shader   = load_shader(GL_VERTEX_SHADER, (char*)v);
    fragment_shader = load_shader(GL_FRAGMENT_SHADER, (char*)f);

    program = glCreateProgram();
    if (program == 0) {
        shader = 0;
        return;
    }

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);

    glLinkProgram(program);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint info_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1) {
            char* info_log = (char*)malloc(sizeof(char) * info_len);
            glGetProgramInfoLog(program, info_len, NULL, info_log);
            fprintf(stderr, "Error linking program:\n%s\n", info_log);
            free(info_log);
        }
        glDeleteProgram(program);
        shader = 0;
        return;
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    shader->id = program;
}

void shader_use(Shader* shader) { glUseProgram(shader->id); }

void shader_set_bool(Shader* shader, const char* name, bool value)
{
    glUniform1i(glGetUniformLocation(shader->id, name), value);
}

void shader_set_int(Shader* shader, const char* name, int32_t value)
{
    glUniform1i(glGetUniformLocation(shader->id, name), value);
}

void shader_set_float(Shader* shader, const char* name, float value)
{
    glUniform1f(glGetUniformLocation(shader->id, name), value);
}

static void* get_proc_address(HMODULE module, const char* proc_name)
{
    void* proc = (void*)wglGetProcAddress(proc_name);
    if (!proc) proc = (void*)GetProcAddress(module, proc_name);

    return proc;
}

static void debug_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
    const GLchar* message, const void* userParam)
{
    char buff[512] = {};
    sprintf(buff, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
        (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity, message);
    non_fatal_error(buff);
}

static void init_opengl_extensions()
{
    // Before we can load extensions, we need a dummy OpenGL context, created using a dummy window.
    // We use a dummy window because you can only set the pixel format for a window once. For the
    // real window, we want to use wglChoosePixelFormatARB (so we can potentially specify options
    // that aren't available in PIXELFORMATDESCRIPTOR), but we can't load and use that before we
    // have a context.
    WNDCLASSA window_class     = { 0 };
    window_class.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc   = DefWindowProcA;
    window_class.hInstance     = GetModuleHandle(0);
    window_class.lpszClassName = "Dummy_WGL_window";

    if (!RegisterClass(&window_class)) { fatal_error("Failed to register dummy OpenGL window."); }

    HWND dummy_window = CreateWindowEx(0, window_class.lpszClassName, "Dummy OpenGL Window", 0, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, window_class.hInstance, 0);

    if (!dummy_window) { fatal_error("Failed to create dummy OpenGL window."); }

    HDC dummy_dc = GetDC(dummy_window);

    PIXELFORMATDESCRIPTOR pfd = { 0 };
    pfd.nSize                 = sizeof(pfd);
    pfd.nVersion              = 1;
    pfd.iPixelType            = PFD_TYPE_RGBA;
    pfd.dwFlags               = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.cColorBits            = 32;
    pfd.cAlphaBits            = 8;
    pfd.iLayerType            = PFD_MAIN_PLANE;
    pfd.cDepthBits            = 24;
    pfd.cStencilBits          = 8;

    int pixel_format = ChoosePixelFormat(dummy_dc, &pfd);
    if (!pixel_format) { fatal_error("Failed to find a suitable pixel format."); }
    if (!SetPixelFormat(dummy_dc, pixel_format, &pfd)) { fatal_error("Failed to set the pixel format."); }

    HGLRC dummy_context = wglCreateContext(dummy_dc);
    if (!dummy_context) { fatal_error("Failed to create a dummy OpenGL rendering context."); }

    if (!wglMakeCurrent(dummy_dc, dummy_context)) { fatal_error("Failed to activate dummy OpenGL rendering context."); }

    HMODULE gl = LoadLibrary(TEXT("opengl32.dll"));

    wglGetExtensionsStringARB  = (PFNWGLGETEXTENSIONSSTRINGARBPROC)get_proc_address(gl, "wglGetExtensionsStringARB");
    wglChoosePixelFormatARB    = (PFNWGLCHOOSEPIXELFORMATARBPROC)get_proc_address(gl, "wglChoosePixelFormatARB");
    wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)get_proc_address(gl, "wglCreateContextAttribsARB");
    glAttachShader             = (PFNGLATTACHSHADERPROC)get_proc_address(gl, "glAttachShader");
    glBindBuffer               = (PFNGLBINDBUFFERPROC)get_proc_address(gl, "glBindBuffer");
    glBindTexture              = (PFNGLBINDTEXTUREPROC)get_proc_address(gl, "glBindTexture");
    glBindVertexArray          = (PFNGLBINDVERTEXARRAYPROC)get_proc_address(gl, "glBindVertexArray");
    glBufferData               = (PFNGLBUFFERDATAPROC)get_proc_address(gl, "glBufferData");
    glClear                    = (PFNGLCLEARPROC)get_proc_address(gl, "glClear");
    glClearColor               = (PFNGLCLEARCOLORPROC)get_proc_address(gl, "glClearColor");
    glCompileShader            = (PFNGLCOMPILESHADERPROC)get_proc_address(gl, "glCompileShader");
    glCreateProgram            = (PFNGLCREATEPROGRAMPROC)get_proc_address(gl, "glCreateProgram");
    glCreateShader             = (PFNGLCREATESHADERPROC)get_proc_address(gl, "glCreateShader");
    glDebugMessageCallback     = (PFNGLDEBUGMESSAGECALLBACKPROC)get_proc_address(gl, "glDebugMessageCallback");
    glDeleteBuffers            = (PFNGLDELETEBUFFERSPROC)get_proc_address(gl, "glDeleteBuffers");
    glDeleteProgram            = (PFNGLDELETEPROGRAMPROC)get_proc_address(gl, "glDeleteProgram");
    glDeleteShader             = (PFNGLDELETESHADERPROC)get_proc_address(gl, "glDeleteShader");
    glDeleteVertexArrays       = (PFNGLDELETEVERTEXARRAYSPROC)get_proc_address(gl, "glDeleteVertexArrays");
    glDrawArrays               = (PFNGLDRAWARRAYSPROC)get_proc_address(gl, "glDrawArrays");
    glDrawElements             = (PFNGLDRAWELEMENTSPROC)get_proc_address(gl, "glDrawElements");
    glEnable                   = (PFNGLENABLEPROC)get_proc_address(gl, "glEnable");
    glEnableVertexAttribArray  = (PFNGLENABLEVERTEXATTRIBARRAYPROC)get_proc_address(gl, "glEnableVertexAttribArray");
    glGenBuffers               = (PFNGLGENBUFFERSPROC)get_proc_address(gl, "glGenBuffers");
    glGenTextures              = (PFNGLGENTEXTURESPROC)get_proc_address(gl, "glGenTextures");
    glGenVertexArrays          = (PFNGLGENVERTEXARRAYSPROC)get_proc_address(gl, "glGenVertexArrays");
    glGenerateMipmap           = (PFNGLGENERATEMIPMAPPROC)get_proc_address(gl, "glGenerateMipmap");
    glGetProgramInfoLog        = (PFNGLGETPROGRAMINFOLOGPROC)get_proc_address(gl, "glGetProgramInfoLog");
    glGetProgramiv             = (PFNGLGETPROGRAMIVPROC)get_proc_address(gl, "glGetProgramiv");
    glGetShaderInfoLog         = (PFNGLGETSHADERINFOLOGPROC)get_proc_address(gl, "glGetShaderInfoLog");
    glGetShaderiv              = (PFNGLGETSHADERIVPROC)get_proc_address(gl, "glGetShaderiv");
    glGetString                = (PFNGLGETSTRINGPROC)get_proc_address(gl, "glGetString");
    glGetUniformLocation       = (PFNGLGETUNIFORMLOCATIONPROC)get_proc_address(gl, "glGetuniformLocation");
    glLinkProgram              = (PFNGLLINKPROGRAMPROC)get_proc_address(gl, "glLinkProgram");
    glShaderSource             = (PFNGLSHADERSOURCEPROC)get_proc_address(gl, "glShaderSource");
    glTexImage2D               = (PFNGLTEXIMAGE2DPROC)get_proc_address(gl, "glTexImage2D");
    glTexParameteri            = (PFNGLTEXPARAMETERIPROC)get_proc_address(gl, "glTexParameteri");
    glUniform1f                = (PFNGLUNIFORM1FPROC)get_proc_address(gl, "glUniform1f");
    glUniform1i                = (PFNGLUNIFORM1IPROC)get_proc_address(gl, "glUniform1i");
    glUseProgram               = (PFNGLUSEPROGRAMPROC)get_proc_address(gl, "glUseProgram");
    glVertexAttribPointer      = (PFNGLVERTEXATTRIBPOINTERPROC)get_proc_address(gl, "glVertexAttribPointer");
    glViewport                 = (PFNGLVIEWPORTPROC)get_proc_address(gl, "glViewport");

    FreeLibrary(gl);

    wglMakeCurrent(dummy_dc, 0);
    wglDeleteContext(dummy_context);
    ReleaseDC(dummy_window, dummy_dc);
    DestroyWindow(dummy_window);
}

static HGLRC init_opengl(HDC real_dc)
{
    init_opengl_extensions();

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(debug_message_callback, 0);

    // Now we can choose a pixel format the modern way, using wglChoosePixelFormatARB.
    int pixel_format_attribs[] = { WGL_DRAW_TO_WINDOW_ARB, GL_TRUE, WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE, WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB, WGL_PIXEL_TYPE_ARB,
        WGL_TYPE_RGBA_ARB, WGL_COLOR_BITS_ARB, 32, WGL_DEPTH_BITS_ARB, 24, WGL_STENCIL_BITS_ARB, 8, 0 };

    int pixel_format;
    UINT num_formats;
    wglChoosePixelFormatARB(real_dc, pixel_format_attribs, 0, 1, &pixel_format, &num_formats);
    if (!num_formats) { fatal_error("Failed to set the OpenGL 3.3 pixel format."); }

    PIXELFORMATDESCRIPTOR pfd;
    DescribePixelFormat(real_dc, pixel_format, sizeof(pfd), &pfd);
    if (!SetPixelFormat(real_dc, pixel_format, &pfd)) { fatal_error("Failed to set the OpenGL 3.3 pixel format."); }

    // Specify that we want to create an OpenGL 3.3 core profile context
    int gl33_attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB,
        3,
        WGL_CONTEXT_MINOR_VERSION_ARB,
        3,
        WGL_CONTEXT_PROFILE_MASK_ARB,
        WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0,
    };

    HGLRC gl33_context = wglCreateContextAttribsARB(real_dc, 0, gl33_attribs);
    if (!gl33_context) { fatal_error("Failed to create OpenGL 3.3 context."); }

    if (!wglMakeCurrent(real_dc, gl33_context)) { fatal_error("Failed to activate OpenGL 3.3 rendering context."); }

    return gl33_context;
}

static LRESULT CALLBACK window_callback(HWND window, UINT msg, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;

    switch (msg) {
    case WM_CLOSE:
        DestroyWindow(window);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        result = DefWindowProc(window, msg, wparam, lparam);
        break;
    }

    return result;
}

static HWND create_window(HINSTANCE inst, int32_t width, int32_t height, const char* title)
{
    WNDCLASSA window_class     = { 0 };
    window_class.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc   = window_callback;
    window_class.hInstance     = inst;
    window_class.hCursor       = LoadCursor(0, IDC_ARROW);
    window_class.hbrBackground = 0;
    window_class.lpszClassName = "WGL_window";

    if (!RegisterClass(&window_class)) { fatal_error("Failed to register window."); }

    // Specify a desired width and height, then adjust the rect so the window's client area will be
    // that size.
    RECT rect   = { 0 };
    rect.right  = width;
    rect.bottom = height;

    DWORD window_style = WS_OVERLAPPEDWINDOW;
    AdjustWindowRect(&rect, window_style, false);

    HWND window = CreateWindowEx(0, window_class.lpszClassName, title, window_style, CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top, 0, 0, inst, 0);

    if (!window) { fatal_error("Failed to create window."); }

    return window;
}

const uint32_t SCR_WIDTH  = 800;
const uint32_t SCR_HEIGHT = 600;

const char* v_shader = "#version 330 core\n"
                       "layout (location = 0) in vec3 aPos;\n"
                       "layout (location = 1) in vec3 aColor;\n"
                       "layout (location = 2) in vec2 aTexCoord;\n"
                       "\n"
                       "out vec3 ourColor;\n"
                       "out vec2 TexCoord;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                       "	gl_Position = vec4(aPos, 1.0);\n"
                       "	ourColor = aColor;\n"
                       "	TexCoord = vec2(aTexCoord.x, aTexCoord.y);\n"
                       "}\n";

const char* f_shader = "#version 330 core\n"
                       "out vec4 FragColor;\n"
                       "\n"
                       "in vec3 ourColor;\n"
                       "in vec2 TexCoord;\n"
                       "\n"
                       "// texture sampler\n"
                       "uniform sampler2D texture1;\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                       "	FragColor = texture(texture1, TexCoord);\n"
                       "}\n";

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd_line, int cmd_show)
{
    HWND window = create_window(inst, SCR_WIDTH, SCR_HEIGHT, "Hello OpenGL");
    HDC dc      = GetDC(window);
    HGLRC rc    = init_opengl(dc);

    // How to deal with resizes?

    Shader shader;
    shader_create(v_shader, f_shader, &shader);

    float vertices[] = {
        // clang-format off
        // positions          // colors           // texture coords
         0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f, // top right
         0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f, // bottom right
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f, // bottom left
        -0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f  // top left
        // clang-format on
    };

    uint32_t indices[] = {
        // clang-format off
        0, 1, 3, // first triangle
        1, 2, 3  // second triangle
        // clang-format on
    };

    uint32_t vbo, vao, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vao);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // texture coord attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    unsigned char* data = stbi_load("resources/container.jpg", &width, &height, &nrChannels, 0);
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        non_fatal_error("Failed to load texture\n");
    }
    stbi_image_free(data);

    ShowWindow(window, cmd_show);
    UpdateWindow(window);

    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        // Update logic here.

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glBindTexture(GL_TEXTURE_2D, texture);

        shader_use(&shader);
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        SwapBuffers(dc);
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);

    wglMakeCurrent(dc, 0);
    wglDeleteContext(rc);
    ReleaseDC(window, dc);
    DestroyWindow(window);

    return 0;
}