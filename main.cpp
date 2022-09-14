#include <windows.h>
#include <GL/glcorearb.h>
#include <GL/wglext.h>

#include <cstdio>
#include <cstdint>
#include <stdlib.h>
#include <string.h>

#define TRUE 1
#define FALSE 0

typedef struct
{
    GLuint program_object;
} UserData;

typedef struct TargetState
{
    uint32_t width;
    uint32_t height;

    Display *x_display;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLNativeWindowType native_window;

    UserData *user_data;
    void (*draw_func)(struct TargetState *);
} TargetState;

TargetState state;
TargetState *p_state = &state;

LRESULT CALLBACK dummy_window_procedure(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK window_procedure(HWND, UINT, WPARAM, LPARAM);
void *get_proc(const char *);
void draw();

HMODULE gl_module;
bool running = false;

/* OpenGL function declarations. In practice, we would put these in a
   separate header file and add "extern" in front, so that we can use them
   anywhere after loading them only once. */
PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB;
PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLCLEARPROC glClear;
PFNGLCLEARCOLORPROC glClearColor;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLCREATESHADERPROC glCreateShader;
PFNGLDELETEPROGRAMPROC glDeleteProgram;
PFNGLDELETESHADERPROC glDeleteShader;
PFNGLDRAWARRAYSPROC glDrawArrays;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
PFNGLGETERRORPROC glGetError;
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

int CALLBACK
WinMain(HINSTANCE instance_handle, HINSTANCE prev_instance_handle, LPSTR cmd_line, int cmd_show)
{
    //////////////////////////////// Register window
    WNDCLASS dummy_window_class;

    ZeroMemory(&dummy_window_class, sizeof(dummy_window_class));

    dummy_window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    dummy_window_class.lpfnWndProc = dummy_window_procedure;
    dummy_window_class.hInstance = instance_handle;
    dummy_window_class.lpszClassName = TEXT("DUMMY_OPENGL_WINDOW");

    RegisterClass(&dummy_window_class);

    ////////////////////////////// Create window
    HWND dummy_window_handle = CreateWindowEx(WS_EX_OVERLAPPEDWINDOW, TEXT("DUMMY_OPENGL_WINDOW"), TEXT("Dummy OpenGL Window"), WS_OVERLAPPEDWINDOW, 0, 0, 800, 600, NULL, NULL, instance_handle, NULL);
    HDC dc = GetDC(dummy_window_handle);

    ShowWindow(dummy_window_handle, SW_HIDE);

    ////////////////////////// Pixel format
    PIXELFORMATDESCRIPTOR descriptor;

    ZeroMemory(&descriptor, sizeof(descriptor));

    descriptor.nSize = sizeof(descriptor);
    descriptor.nVersion = 1;
    descriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL | PFD_GENERIC_ACCELERATED | PFD_DOUBLEBUFFER | PFD_SWAP_LAYER_BUFFERS;
    descriptor.iPixelType = PFD_TYPE_RGBA;
    descriptor.cColorBits = 32;
    descriptor.cRedBits = 8;
    descriptor.cGreenBits = 8;
    descriptor.cBlueBits = 8;
    descriptor.cAlphaBits = 8;
    descriptor.cDepthBits = 32;
    descriptor.cStencilBits = 8;

    int pixel_format = ChoosePixelFormat(dc, &descriptor);
    SetPixelFormat(dc, pixel_format, &descriptor);

    //////////////////////////////// Rendering context
    HGLRC rc = wglCreateContext(dc);
    wglMakeCurrent(dc, rc);

    ////////////////// LOAD functions - should probably be own procedure
    gl_module = LoadLibrary(TEXT("opengl32.dll"));

    wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)get_proc("wglGetExtensionsStringARB");
    wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)get_proc("wglChoosePixelFormatARB");
    wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)get_proc("wglCreateContextAttribsARB");
    glAttachShader = (PFNGLATTACHSHADERPROC)get_proc("glAttachShader");
    glClear = (PFNGLCLEARPROC)get_proc("glClear");
    glClearColor = (PFNGLCLEARCOLORPROC)get_proc("glClearColor");
    glCompileShader = (PFNGLCOMPILESHADERPROC)get_proc("glCompileShader");
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)get_proc("glCreateProgram");
    glCreateShader = (PFNGLCREATESHADERPROC)get_proc("glCreateShader");
    glDeleteProgram = (PFNGLDELETEPROGRAMPROC)get_proc("glDeleteProgram");
    glDeleteShader = (PFNGLDELETESHADERPROC)get_proc("glDeleteShader");
    glDrawArrays = (PFNGLDRAWARRAYSPROC)get_proc("glDrawArrays");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)get_proc("glEnableVertexAttribArray");
    glGetError = (PFNGLGETERRORPROC)get_proc("glGetError");
    glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)get_proc("glGetProgramInfoLog");
    glGetProgramiv = (PFNGLGETPROGRAMIVPROC)get_proc("glGetProgramiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)get_proc("glGetShaderInfoLog");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)get_proc("glGetShaderiv");
    glGetString = (PFNGLGETSTRINGPROC)get_proc("glGetString");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)get_proc("glLinkProgram");
    glShaderSource = (PFNGLSHADERSOURCEPROC)get_proc("glShaderSource");
    glUseProgram = (PFNGLUSEPROGRAMPROC)get_proc("glUseProgram");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)get_proc("glVertexAttribPointer");
    glViewport = (PFNGLVIEWPORTPROC)get_proc("glViewport");

    FreeLibrary(gl_module);

    const GLubyte *version = glGetString(GL_VERSION);
    printf("%s\n", version);
    fflush(stdout);

    /////////////////// New Pixel Format
    int pixel_format_arb;
    UINT pixel_formats_found;

    int pixel_attributes[] = {
        WGL_SUPPORT_OPENGL_ARB, 1,
        WGL_DRAW_TO_WINDOW_ARB, 1,
        WGL_DRAW_TO_BITMAP_ARB, 1,
        WGL_DOUBLE_BUFFER_ARB, 1,
        WGL_SWAP_LAYER_BUFFERS_ARB, 1,
        WGL_COLOR_BITS_ARB, 32,
        WGL_RED_BITS_ARB, 8,
        WGL_GREEN_BITS_ARB, 8,
        WGL_BLUE_BITS_ARB, 8,
        WGL_ALPHA_BITS_ARB, 8,
        WGL_DEPTH_BITS_ARB, 32,
        WGL_STENCIL_BITS_ARB, 8,
        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        0};

    BOOL result = wglChoosePixelFormatARB(dc, pixel_attributes, NULL, 1, &pixel_format_arb, &pixel_formats_found);

    if (!result)
    {
        printf("Could not find pixel format\n");
        fflush(stdout);
        return 0;
    }

    //////////////////// Recreate window
    wglMakeCurrent(dc, NULL);
    wglDeleteContext(rc);
    ReleaseDC(dummy_window_handle, dc);
    DestroyWindow(dummy_window_handle);
    UnregisterClass(TEXT("DUMMY_OPENGL_WINDOW"), instance_handle);

    WNDCLASS window_class;

    ZeroMemory(&window_class, sizeof(window_class));

    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = window_procedure;
    window_class.hInstance = instance_handle;
    window_class.lpszClassName = TEXT("OPENGL_WINDOW");

    RegisterClass(&window_class);

    HWND window_handle = CreateWindowEx(WS_EX_OVERLAPPEDWINDOW,
                                        TEXT("OPENGL_WINDOW"),
                                        TEXT("OpenGL window"),
                                        WS_OVERLAPPEDWINDOW,
                                        0, 0,
                                        800, 600,
                                        NULL,
                                        NULL,
                                        instance_handle,
                                        NULL);

    dc = GetDC(window_handle);

    ShowWindow(window_handle, SW_SHOW);

    //////////////// New context
    GLint context_attributes[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0};

    rc = wglCreateContextAttribsARB(dc, 0, context_attributes);
    wglMakeCurrent(dc, rc);

    //////////////////// Event pump
    running = true;
    MSG msg;
    while (running)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                running = false;
                break;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        draw();
        SwapBuffers(dc);
    }

    wglMakeCurrent(dc, NULL);
    wglDeleteContext(rc);
    ReleaseDC(window_handle, dc);
    DestroyWindow(window_handle);
    UnregisterClass(TEXT("OPENGL_WINDOW"), instance_handle);

    return 0;
}

LRESULT CALLBACK dummy_window_procedure(HWND window_handle, UINT message, WPARAM param_w, LPARAM param_l)
{
    switch (message)
    {
    case WM_DESTROY:
        return 0;
    }

    return DefWindowProc(window_handle, message, param_w, param_l);
}

LRESULT CALLBACK window_procedure(HWND window_handle, UINT message, WPARAM param_w, LPARAM param_l)
{
    switch (message)
    {
    case WM_CLOSE:
    {
        DestroyWindow(window_handle);
        break;
    }
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProc(window_handle, message, param_w, param_l);
}

/* A procedure for getting OpenGL functions and OpenGL or WGL extensions.
   When looking for OpenGL 1.2 and above, or extensions, it uses wglGetProcAddress,
   otherwise it falls back to GetProcAddress. */
void *get_proc(const char *proc_name)
{
    void *proc = (void *)wglGetProcAddress(proc_name);
    if (!proc)
        proc = (void *)GetProcAddress(gl_module, proc_name);

    return proc;
}

void draw() {}

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

GLuint load_shader(GLenum type, const char *shader_src)
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

int init(TargetState *p_state)
{
    p_state->user_data = (UserData *)malloc(sizeof(UserData));

    GLbyte v_shader_str[] = "#version 300 es\n"
                            "layout (location = 0) in vec4 v_Position;\n"
                            "void main()\n"
                            "{\n"
                            "    gl_Position = v_Position;\n"
                            "}\n";

    GLbyte f_shader_str[] = "#version 300 es\n"
                            "precision mediump float;\n"
                            "out vec4 frag_color;\n"
                            "void main()\n"
                            "{\n"
                            "    frag_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
                            "}\n";

    GLuint program_object, vertex_shader, fragment_shader;

    vertex_shader = load_shader(GL_VERTEX_SHADER, (char *)v_shader_str);
    fragment_shader = load_shader(GL_FRAGMENT_SHADER, (char *)f_shader_str);

    program_object = glCreateProgram();
    if (program_object == 0)
    {
        return 0;
    }

    glAttachShader(program_object, vertex_shader);
    glAttachShader(program_object, fragment_shader);

    glLinkProgram(program_object);

    GLint linked;
    glGetProgramiv(program_object, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLint info_len = 0;
        glGetProgramiv(program_object, GL_INFO_LOG_LENGTH, &info_len);
        if (info_len > 1)
        {
            char *info_log = (char *)malloc(sizeof(char) * info_len);
            glGetProgramInfoLog(program_object, info_len, NULL, info_log);
            fprintf(stderr, "Error linking program:\n%s\n", info_log);
            free(info_log);
        }
        glDeleteProgram(program_object);
        return FALSE;
    }

    p_state->user_data->program_object = program_object;

    return TRUE;
}

void init_ogl(TargetState *state, int width, int height)
{
    state->width = width;
    state->height = height;

    EGLint num_configs;
    EGLint major_version;
    EGLint minor_version;

    EGLDisplay display;
    EGLContext context;
    EGLSurface surface;
    EGLConfig config;
    EGLint context_atttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE};

    Window root;
    XSetWindowAttributes swa;
    XSetWindowAttributes xattr;
    Atom wm_state;
    XWMHints hints;
    XEvent xev;
    EGLConfig ecfg;
    EGLint num_config;
    Window win;
    Screen *screen;

    Display *x_display = state->x_display;
    x_display = XOpenDisplay(NULL);
    if (x_display == NULL)
    {
        printf("Sorry to say we can't create an Xwindow and this will fail");
        exit(0); // return; we need to trap this.
    }
    eglBindAPI(EGL_OPENGL_ES_API);
    root = DefaultRootWindow(x_display);
    screen = ScreenOfDisplay(x_display, 0);

    state->width = width;
    state->height = height;

    swa.event_mask = ExposureMask | PointerMotionMask | KeyPressMask | KeyReleaseMask;
    swa.background_pixmap = None;
    swa.background_pixel = 0;
    swa.border_pixel = 0;
    swa.override_redirect = TRUE;

    win = XCreateWindow(x_display, root, 0, 0, width, height, 0, CopyFromParent, InputOutput,
                        CopyFromParent, CWEventMask, &swa);

    XSelectInput(x_display, win, KeyPressMask | KeyReleaseMask);

    xattr.override_redirect = TRUE;
    XChangeWindowAttributes(x_display, win, CWOverrideRedirect, &xattr);

    hints.input = TRUE;
    hints.flags = InputHint;
    XSetWMHints(x_display, win, &hints);

    char *title = (char *)"x11 window Triangle Example";
    // Make the window visible on the screen
    XMapWindow(x_display, win);
    XStoreName(x_display, win, title);

    // Get identifiers for the provided atom name strings
    wm_state = XInternAtom(x_display, "_NET_WM_STATE", FALSE);

    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.window = win;
    xev.xclient.message_type = wm_state;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 1;
    xev.xclient.data.l[1] = FALSE;
    XSendEvent(x_display, DefaultRootWindow(x_display), FALSE, SubstructureNotifyMask, &xev);

    state->native_window = (EGLNativeWindowType)win;

    // Get display
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY)
    {
        printf("Sorry to say we have an EGLinit error and this will fail");
        EGLint err = eglGetError();
        return; // EGL_FALSE;
    }

    // Initialise EGL
    if (!eglInitialize(display, &major_version, &minor_version))
    {
        printf("Sorry to say we have an EGLinit error and this will fail");
        EGLint err = eglGetError();
        return; // EGL_FALSE;
    }

    // Get configs
    if (!eglGetConfigs(display, NULL, 0, &num_configs))
    {
        printf("Sorry to say we have EGL config errors and this will fail");
        EGLint err = eglGetError();
        return; // EGL_FALSE;
    }

    // Choose config
    if (!eglChooseConfig(display, attribute_list, &config, 1, &num_configs))
    {
        printf("Sorry to say we have config choice issues and this will fail");
        EGLint err = eglGetError();
        return; // EGL_FALSE;
    }

    // Create a surface
    surface = eglCreateWindowSurface(display, config, state->native_window, NULL);
    if (surface == EGL_NO_SURFACE)
    {
        EGLint err = eglGetError();
        return; // EGL_FALSE;
    }

    // Create a GL context
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_atttribs);
    if (context == EGL_NO_CONTEXT)
    {
        EGLint err = eglGetError();
        return; // EGL_FALSE;
    }

    // Make the context current
    if (!eglMakeCurrent(display, surface, surface, context))
    {
        EGLint err = eglGetError();
        return; // EGL_FALSE;
    }

    state->display = display;
    state->surface = surface;
    state->context = context;

    // just for fun lets see what we can do with this GPU
    printf("This SBC supports version %i.%i of EGL\n", major_version, minor_version);
    printf("This GPU supplied by  :%s\n", glGetString(GL_VENDOR));
    printf("This GPU supports     :%s\n", glGetString(GL_VERSION));
    printf("This GPU Renders with :%s\n", glGetString(GL_RENDERER));
    printf("This GPU supports     :%s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("This GPU supports these extensions    :%s\n", glGetString(GL_EXTENSIONS));
    fflush(stdout);

    EGLBoolean gl_test = eglGetConfigAttrib(display, config, EGL_MAX_SWAP_INTERVAL, &minor_version);

    // 1 to lock speed to 60fps (assuming we are able to maintain it)
    // 0 for immediate swap (may cause tearing) which will indicate actual frame rate
    EGLBoolean test = eglSwapInterval(display, 1);

    if (glGetError() == GL_NO_ERROR)
    {
        return;
    }
    else
    {
        printf("Oh bugger, Some part of the EGL/OGL graphic init failed\n");
    }
}

void draw(TargetState *p_state)
{
    UserData *user_data = p_state->user_data;
    GLfloat triangle_vertices[] = {0.0f, 0.5f, 0.0f, -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f};

    glViewport(0, 0, p_state->width, p_state->height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(user_data->program_object);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, triangle_vertices);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (glGetError() != GL_NO_ERROR)
    {
        printf("Oh bugger");
    }
}

void es_init_context(TargetState *p_state)
{
    if (p_state != NULL)
    {
        memset(p_state, 0, sizeof(TargetState));
    }
}

void es_register_draw_func(TargetState *p_state, void (*draw_func)(TargetState *))
{
    p_state->draw_func = draw_func;
}

void es_main_loop(TargetState *es_context)
{
    int counter = 0;
    while (counter++ < 200)
    {
        if (es_context->draw_func != NULL)
        {
            es_context->draw_func(es_context);
        }
        eglSwapBuffers(es_context->display, es_context->surface);
    }
}

int main(int argc, char *argv[])
{
    UserData user_data;
    es_init_context(p_state);
    init_ogl(p_state, 1024, 720);
    p_state->user_data = &user_data;

    if (!init(p_state))
    {
        return 0;
    }
    es_register_draw_func(p_state, draw);

    es_main_loop(p_state);
}
