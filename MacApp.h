/* From https://riptutorial.com/opengl */

#pragma once

#import "App.h"

#import <Cocoa/Cocoa.h>

#import <memory>

NSApplication *application;

@interface MacApp : NSWindow <NSApplicationDelegate> {
  std::shared_ptr<Application> appInstance;
}

@property(nonatomic, retain) NSOpenGLView *glView;
- (void)drawLoop:(NSTimer *)timer;
@end
