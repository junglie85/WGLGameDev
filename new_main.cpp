/* Based on https://gist.github.com/nickrolfe/1127313ed1dbf80254b614a721b3ee9c */
/* and https://riptutorial.com/opengl/example/5305/manual-opengl-setup-on-windows */

#include <windows.h>
#include <GL/glcorearb.h>
#include <GL/wglext.h>

#include <cstdio>
#include <cstdint>

PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB;
PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
PFNGLBUFFERDATAPROC glBufferData;
PFNGLCLEARPROC glClear;
PFNGLCLEARCOLORPROC glClearColor;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLCREATESHADERPROC glCreateShader;
PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback;
PFNGLDELETEPROGRAMPROC glDeleteProgram;
PFNGLDELETESHADERPROC glDeleteShader;
PFNGLDRAWARRAYSPROC glDrawArrays;
PFNGLENABLEPROC glEnable;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
PFNGLGENBUFFERSPROC glGenBuffers;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
PFNGLGETPROGRAMIVPROC glGetProgramiv;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
PFNGLGETSHADERIVPROC glGetShaderiv;
PFNGLGETSTRINGPROC glGetString;
PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLUSEPROGRAMPROC glUseProgram;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
PFNGLVIEWPORTPROC glViewport;

typedef struct
{
    GLuint program;
    GLuint vao;
    GLuint vbo;
} UserData;

typedef struct TargetState
{
    int32_t width;
    int32_t height;

    HWND window;
    HDC dc;
    HGLRC rc;

    UserData *user_data;
    void (*draw_func)(struct TargetState *);
} TargetState;

static void fatal_error(const char *msg);
static void *get_proc_address(HMODULE module, const char *proc_name);
static void debug_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam);
static void init_opengl_extensions();
static HGLRC init_opengl(HDC real_dc);
static LRESULT CALLBACK window_callback(HWND window, UINT msg, WPARAM wparam, LPARAM lparam);
static HWND create_window(HINSTANCE inst, int32_t width, int32_t height);
static void deinit_opengl(TargetState *state);
static bool init(TargetState *state);
static GLuint load_shader(GLenum type, const char *shader_src);
static void draw(TargetState *state);

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd_line, int cmd_show)
{
    UserData user_data = {0};
    TargetState state = {0};
    state.user_data = &user_data;
    state.width = 1024;
    state.height = 576;

    state.window = create_window(inst, state.width, state.height);
    state.dc = GetDC(state.window);
    state.rc = init_opengl(state.dc);
    if (!init(&state))
    {
        fatal_error("Failed to initialise user data.");
    }

    // Should probably be in init?
    GLfloat triangle_vertices[] = {0.0f, 0.5f, 0.0f, -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f};

    glGenBuffers(1, &user_data.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, user_data.vbo);
    glBufferData(GL_ARRAY_BUFFER, 9 * sizeof(float), triangle_vertices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &user_data.vao);
    glBindVertexArray(user_data.vao);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, NULL);
    glEnableVertexAttribArray(0);

    ShowWindow(state.window, cmd_show);
    UpdateWindow(state.window);

    bool running = true;
    while (running)
    {
        MSG msg;
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                running = false;
            }
            else
            {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
        }

        // Do OpenGL rendering here
        draw(&state);
        SwapBuffers(state.dc);
    }

    deinit_opengl(&state);
    state.user_data = NULL;

    return 0;
}

static void fatal_error(const char *msg)
{
    MessageBoxA(NULL, msg, "Error", MB_OK | MB_ICONEXCLAMATION);
    exit(EXIT_FAILURE);
}

static void *get_proc_address(HMODULE module, const char *proc_name)
{
    void *proc = (void *)wglGetProcAddress(proc_name);
    if (!proc)
        proc = (void *)GetProcAddress(module, proc_name);

    return proc;
}

static void debug_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
    char buff[512] = {};
    sprintf(buff, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
            (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
            type, severity, message);
    OutputDebugString(TEXT(buff));
}

static void init_opengl_extensions()
{
    // Before we can load extensions, we need a dummy OpenGL context, created using a dummy window.
    // We use a dummy window because you can only set the pixel format for a window once. For the
    // real window, we want to use wglChoosePixelFormatARB (so we can potentially specify options
    // that aren't available in PIXELFORMATDESCRIPTOR), but we can't load and use that before we
    // have a context.
    WNDCLASSA window_class = {0};
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = DefWindowProcA;
    window_class.hInstance = GetModuleHandle(0);
    window_class.lpszClassName = "Dummy_WGL_djuasiodwa";

    if (!RegisterClassA(&window_class))
    {
        fatal_error("Failed to register dummy OpenGL window.");
    }

    HWND dummy_window = CreateWindowExA(
        0,
        window_class.lpszClassName,
        "Dummy OpenGL Window",
        0,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        0,
        0,
        window_class.hInstance,
        0);

    if (!dummy_window)
    {
        fatal_error("Failed to create dummy OpenGL window.");
    }

    HDC dummy_dc = GetDC(dummy_window);

    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;

    int pixel_format = ChoosePixelFormat(dummy_dc, &pfd);
    if (!pixel_format)
    {
        fatal_error("Failed to find a suitable pixel format.");
    }
    if (!SetPixelFormat(dummy_dc, pixel_format, &pfd))
    {
        fatal_error("Failed to set the pixel format.");
    }

    HGLRC dummy_context = wglCreateContext(dummy_dc);
    if (!dummy_context)
    {
        fatal_error("Failed to create a dummy OpenGL rendering context.");
    }

    if (!wglMakeCurrent(dummy_dc, dummy_context))
    {
        fatal_error("Failed to activate dummy OpenGL rendering context.");
    }

    HMODULE gl = LoadLibrary(TEXT("opengl32.dll"));

    wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)get_proc_address(gl, "wglGetExtensionsStringARB");
    wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)get_proc_address(gl, "wglChoosePixelFormatARB");
    wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)get_proc_address(gl, "wglCreateContextAttribsARB");
    glAttachShader = (PFNGLATTACHSHADERPROC)get_proc_address(gl, "glAttachShader");
    glBindBuffer = (PFNGLBINDBUFFERPROC)get_proc_address(gl, "glBindBuffer");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)get_proc_address(gl, "glBindVertexArray");
    glBufferData = (PFNGLBUFFERDATAPROC)get_proc_address(gl, "glBufferData");
    glClear = (PFNGLCLEARPROC)get_proc_address(gl, "glClear");
    glClearColor = (PFNGLCLEARCOLORPROC)get_proc_address(gl, "glClearColor");
    glCompileShader = (PFNGLCOMPILESHADERPROC)get_proc_address(gl, "glCompileShader");
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)get_proc_address(gl, "glCreateProgram");
    glCreateShader = (PFNGLCREATESHADERPROC)get_proc_address(gl, "glCreateShader");
    glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC)get_proc_address(gl, "glDebugMessageCallback");
    glDeleteProgram = (PFNGLDELETEPROGRAMPROC)get_proc_address(gl, "glDeleteProgram");
    glDeleteShader = (PFNGLDELETESHADERPROC)get_proc_address(gl, "glDeleteShader");
    glDrawArrays = (PFNGLDRAWARRAYSPROC)get_proc_address(gl, "glDrawArrays");
    glEnable = (PFNGLENABLEPROC)get_proc_address(gl, "glEnable");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)get_proc_address(gl, "glEnableVertexAttribArray");
    glGenBuffers = (PFNGLGENBUFFERSPROC)get_proc_address(gl, "glGenBuffers");
    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)get_proc_address(gl, "glGenVertexArrays");
    glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)get_proc_address(gl, "glGetProgramInfoLog");
    glGetProgramiv = (PFNGLGETPROGRAMIVPROC)get_proc_address(gl, "glGetProgramiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)get_proc_address(gl, "glGetShaderInfoLog");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)get_proc_address(gl, "glGetShaderiv");
    glGetString = (PFNGLGETSTRINGPROC)get_proc_address(gl, "glGetString");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)get_proc_address(gl, "glLinkProgram");
    glShaderSource = (PFNGLSHADERSOURCEPROC)get_proc_address(gl, "glShaderSource");
    glUseProgram = (PFNGLUSEPROGRAMPROC)get_proc_address(gl, "glUseProgram");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)get_proc_address(gl, "glVertexAttribPointer");
    glViewport = (PFNGLVIEWPORTPROC)get_proc_address(gl, "glViewport");

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
    int pixel_format_attribs[] = {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB, 32,
        WGL_DEPTH_BITS_ARB, 24,
        WGL_STENCIL_BITS_ARB, 8,
        0};

    int pixel_format;
    UINT num_formats;
    wglChoosePixelFormatARB(real_dc, pixel_format_attribs, 0, 1, &pixel_format, &num_formats);
    if (!num_formats)
    {
        fatal_error("Failed to set the OpenGL 3.3 pixel format.");
    }

    PIXELFORMATDESCRIPTOR pfd;
    DescribePixelFormat(real_dc, pixel_format, sizeof(pfd), &pfd);
    if (!SetPixelFormat(real_dc, pixel_format, &pfd))
    {
        fatal_error("Failed to set the OpenGL 3.3 pixel format.");
    }

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
    if (!gl33_context)
    {
        fatal_error("Failed to create OpenGL 3.3 context.");
    }

    if (!wglMakeCurrent(real_dc, gl33_context))
    {
        fatal_error("Failed to activate OpenGL 3.3 rendering context.");
    }

    return gl33_context;
}

static LRESULT CALLBACK window_callback(HWND window, UINT msg, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;

    switch (msg)
    {
    case WM_CLOSE:
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        result = DefWindowProcA(window, msg, wparam, lparam);
        break;
    }

    return result;
}

static HWND create_window(HINSTANCE inst, int32_t width, int32_t height)
{
    WNDCLASSA window_class = {0};
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = window_callback;
    window_class.hInstance = inst;
    window_class.hCursor = LoadCursor(0, IDC_ARROW);
    window_class.hbrBackground = 0;
    window_class.lpszClassName = "WGL_fdjhsklf";

    if (!RegisterClassA(&window_class))
    {
        fatal_error("Failed to register window.");
    }

    // Specify a desired width and height, then adjust the rect so the window's client area will be
    // that size.
    RECT rect = {0};
    rect.right = width;
    rect.bottom = height;

    DWORD window_style = WS_OVERLAPPEDWINDOW;
    AdjustWindowRect(&rect, window_style, false);

    HWND window = CreateWindowExA(
        0,
        window_class.lpszClassName,
        "OpenGL",
        window_style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        0,
        0,
        inst,
        0);

    if (!window)
    {
        fatal_error("Failed to create window.");
    }

    return window;
}

static void deinit_opengl(TargetState *state)
{
    wglMakeCurrent(state->dc, 0);
    wglDeleteContext(state->rc);
    ReleaseDC(state->window, state->dc);
    DestroyWindow(state->window);
}

static bool init(TargetState *state)
{
    GLbyte v_shader_str[] = "#version 330 core\n"
                            "layout (location = 0) in vec4 v_Position;\n"
                            "void main()\n"
                            "{\n"
                            "    gl_Position = v_Position;\n"
                            "}\n";

    GLbyte f_shader_str[] = "#version 330 core\n"
                            "out vec4 frag_color;\n"
                            "void main()\n"
                            "{\n"
                            "    frag_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
                            "}\n";

    GLuint program, vertex_shader, fragment_shader;

    vertex_shader = load_shader(GL_VERTEX_SHADER, (char *)v_shader_str);
    fragment_shader = load_shader(GL_FRAGMENT_SHADER, (char *)f_shader_str);

    program = glCreateProgram();
    if (program == 0)
    {
        return 0;
    }

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);

    glLinkProgram(program);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLint info_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1)
        {
            char *info_log = (char *)malloc(sizeof(char) * info_len);
            glGetProgramInfoLog(program, info_len, NULL, info_log);
            fprintf(stderr, "Error linking program:\n%s\n", info_log);
            free(info_log);
        }
        glDeleteProgram(program);
        return FALSE;
    }

    state->user_data->program = program;

    return true;
}

static GLuint load_shader(GLenum type, const char *shader_src)
{
    GLuint shader = glCreateShader(type);

    if (shader == 0)
    {
        return 0;
    }
    glShaderSource(shader, 1, &shader_src, NULL);
    glCompileShader(shader);

    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        GLint info_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1)
        {
            char *info_log = (char *)malloc(sizeof(char) * info_len);
            glGetShaderInfoLog(shader, info_len, NULL, info_log);
            fprintf(stderr, "Error compiling this shader:\n%s\n", info_log);
            free(info_log);
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static void draw(TargetState *state)
{
    glViewport(0, 0, state->width, state->height);
    // glClearColor(1.0f, 0.5f, 0.5f, 1.0f);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(state->user_data->program);
    glBindVertexArray(state->user_data->vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}
