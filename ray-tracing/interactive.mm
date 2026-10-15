#import <Cocoa/Cocoa.h>
#include "realtime_renderer.h"
#include <chrono>
#include <cstring>
#include <sstream>

@interface RayView : NSImageView
@property(nonatomic) RtRenderer* renderer;
@property(nonatomic) RtCamera* camera;
@property(nonatomic) NSSlider* spheres;
@property(nonatomic) NSSlider* sunAzimuth;
@property(nonatomic) NSSlider* sunElevation;
@property(nonatomic) NSTextField* readout;
- (void)drawNextFrame:(NSTimer*)timer;
- (void)controlsChanged:(id)sender;
@end

@implementation RayView
- (BOOL)acceptsFirstResponder { return YES; }
- (void)updateSun {
    float azimuth=self.sunAzimuth.floatValue*.0174532925f, elevation=self.sunElevation.floatValue*.0174532925f;
    self.renderer->setSunDirection(rt_vec3(std::cos(elevation)*std::cos(azimuth),std::sin(elevation),std::cos(elevation)*std::sin(azimuth)));
}
- (void)controlsChanged:(id)sender { if(sender==self.spheres) self.renderer->rebuildScene((int)self.spheres.integerValue); [self updateSun]; }
- (void)drawNextFrame:(NSTimer*)timer {
    auto start=std::chrono::steady_clock::now(); self.renderer->render(*self.camera);
    int width=self.renderer->settings().width,height=self.renderer->settings().height;
    NSBitmapImageRep* bitmap=[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL pixelsWide:width pixelsHigh:height bitsPerSample:8 samplesPerPixel:3 hasAlpha:NO isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bitmapFormat:0 bytesPerRow:width*3 bitsPerPixel:24];
    std::memcpy(bitmap.bitmapData,self.renderer->pixels().data(),self.renderer->pixels().size());
    NSImage* image=[[NSImage alloc] initWithSize:NSMakeSize(width,height)]; [image addRepresentation:bitmap]; self.image=image;
    float ms=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-start).count(); std::ostringstream text;
    text << "Spheres: " << self.renderer->settings().sphere_count << "\nCPU frame: " << int(ms) << " ms\nBuffer: " << self.renderer->memoryBytes()/1024 << " KiB";
    self.readout.stringValue=[NSString stringWithUTF8String:text.str().c_str()];
}
- (void)scrollWheel:(NSEvent*)event { self.camera->distance=rt_clamp(self.camera->distance-event.scrollingDeltaY*.06f,3,42); }
- (void)keyDown:(NSEvent*)event {
    NSString* key=event.charactersIgnoringModifiers.lowercaseString; float step=(event.modifierFlags&NSEventModifierFlagShift)?.18f:.075f;
    if([key isEqualToString:@"a"]||event.keyCode==123) self.camera->yaw-=step;
    else if([key isEqualToString:@"d"]||event.keyCode==124) self.camera->yaw+=step;
    else if([key isEqualToString:@"w"]||event.keyCode==126) self.camera->pitch=rt_clamp(self.camera->pitch+step,.08f,1.22f);
    else if([key isEqualToString:@"s"]||event.keyCode==125) self.camera->pitch=rt_clamp(self.camera->pitch-step,.08f,1.22f);
    else if([key isEqualToString:@"q"]||[key isEqualToString:@"-"]) self.camera->distance=rt_clamp(self.camera->distance+1,3,42);
    else if([key isEqualToString:@"e"]||[key isEqualToString:@"+"]||[key isEqualToString:@"="]) self.camera->distance=rt_clamp(self.camera->distance-1,3,42);
    else if([key isEqualToString:@"r"]) self.renderer->randomizeScene(); else [super keyDown:event];
}
@end

static NSTextField* label(NSString* text,NSRect frame) { NSTextField* field=[[NSTextField alloc] initWithFrame:frame]; field.stringValue=text; field.editable=NO; field.bordered=NO; field.drawsBackground=NO; return field; }
@interface AppDelegate : NSObject <NSApplicationDelegate> @end
@implementation AppDelegate
- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    // Render below the displayed size to leave frame-time headroom on integrated GPUs/low-RAM Macs.
    RtSettings settings; settings.width=640; settings.height=448; settings.sphere_count=120;
    auto* renderer=new RtRenderer(settings); auto* camera=new RtCamera();
    NSWindow* window=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,1040,620) styleMask:(NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable) backing:NSBackingStoreBuffered defer:NO]; window.title=@"Ray Tracer — live CPU view";
    NSView* content=window.contentView; RayView* view=[[RayView alloc] initWithFrame:NSMakeRect(10,25,800,560)]; view.imageScaling=NSImageScaleAxesIndependently; view.renderer=renderer; view.camera=camera; [content addSubview:view];
    CGFloat x=830; [content addSubview:label(@"LIVE CONTROLS",NSMakeRect(x,555,180,24))]; [content addSubview:label(@"Sphere count",NSMakeRect(x,500,180,18))];
    NSSlider* spheres=[NSSlider sliderWithValue:120 minValue:0 maxValue:600 target:view action:@selector(controlsChanged:)]; spheres.frame=NSMakeRect(x,475,180,24); view.spheres=spheres; [content addSubview:spheres];
    [content addSubview:label(@"Sun azimuth",NSMakeRect(x,425,180,18))]; NSSlider* azimuth=[NSSlider sliderWithValue:148 minValue:-180 maxValue:180 target:view action:@selector(controlsChanged:)]; azimuth.frame=NSMakeRect(x,400,180,24); view.sunAzimuth=azimuth; [content addSubview:azimuth];
    [content addSubview:label(@"Sun elevation",NSMakeRect(x,350,180,18))]; NSSlider* elevation=[NSSlider sliderWithValue:55 minValue:5 maxValue:85 target:view action:@selector(controlsChanged:)]; elevation.frame=NSMakeRect(x,325,180,24); view.sunElevation=elevation; [content addSubview:elevation];
    NSTextField* info=label(@"",NSMakeRect(x,250,190,58)); view.readout=info; [content addSubview:info]; [content addSubview:label(@"WASD / arrows: orbit\nScroll or Q/E: zoom\nR: new terrain",NSMakeRect(x,150,190,70))];
    [window makeKeyAndOrderFront:nil]; [window makeFirstResponder:view]; [view controlsChanged:nil]; [NSTimer scheduledTimerWithTimeInterval:1.0/24.0 target:view selector:@selector(drawNextFrame:) userInfo:nil repeats:YES];
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender { return YES; }
@end
int main(int, const char*[]) { @autoreleasepool { NSApplication* app=[NSApplication sharedApplication]; AppDelegate* delegate=[[AppDelegate alloc] init]; app.delegate=delegate; [app setActivationPolicy:NSApplicationActivationPolicyRegular]; [app run]; } return 0; }
