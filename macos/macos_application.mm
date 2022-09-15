#import "macos_application.h"

int main(int argc, const char* argv[])
{
    MacApp* app;
    application = [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    app = [[MacApp alloc]
        initWithContentRect:NSMakeRect(0, 0, 600, 600)
                  styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable
                    backing:NSBackingStoreBuffered
                      defer:YES];
    [application setDelegate:app];
    [application run];
}

@implementation MacApp

@synthesize glView;

BOOL shouldStop = NO;

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSUInteger)aStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)flag
{
    if (self = [super initWithContentRect:contentRect styleMask:aStyle backing:bufferingType defer:flag]) {
        // sets the title of the window (Declared in Plist)
        [self setTitle:[[NSProcessInfo processInfo] processName]];

        // This is pretty important.. OS X starts always with a context that only
        // supports openGL 2.1 This will ditch the classic OpenGL and initialises
        // openGL 4.1
        NSOpenGLPixelFormatAttribute pixelFormatAttributes[]
            = { NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core, NSOpenGLPFAColorSize, 24, NSOpenGLPFAAlphaSize,
                  8, NSOpenGLPFADoubleBuffer, NSOpenGLPFAAccelerated, NSOpenGLPFANoRecovery, 0 };

        NSOpenGLPixelFormat* format = [[NSOpenGLPixelFormat alloc] initWithAttributes:pixelFormatAttributes];
        // Initialize the view
        glView = [[NSOpenGLView alloc] initWithFrame:contentRect pixelFormat:format];

        // Set context and attach it to the window
        [[glView openGLContext] makeCurrentContext];

        // finishing off
        [self setContentView:glView];
        [glView prepareOpenGL];
        [self makeKeyAndOrderFront:self];
        [self setAcceptsMouseMovedEvents:YES];
        [self makeKeyWindow];
        [self setOpaque:YES];

        // Start the c++ code
        appInstance = std::shared_ptr<Application>(new Application());
    }

    return self;
}

- (void)drawLoop:(NSTimer*)timer
{
    if (shouldStop) {
        [self close];
        return;
    }
    if ([self isVisible]) {
        appInstance->update();
        [glView update];
        [[glView openGLContext] flushBuffer];
    }
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification
{
    [NSTimer scheduledTimerWithTimeInterval:0.000001
                                     target:self
                                   selector:@selector(drawLoop:)
                                   userInfo:nil
                                    repeats:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)theApplication
{
    return YES;
}

- (void)applicationWillTerminate:(NSNotification*)aNotification
{
    shouldStop = YES;
}

@end
