// Use a loopback RTSP test stream with fixed dimensions. Exercise the same native
// player used by wxMediaCtrl2, including reopening after an explicit close.
#import "slic3r/GUI/BambuPlayer/BambuPlayer.h"
#include <atomic>
#include <dlfcn.h>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 3 && argc != 4) {
        std::cerr << "Usage: native_camera libBambuSource.dylib LOCAL_BAMBU_URL\n";
        return 2;
    }
    @autoreleasepool {
        [NSApplication sharedApplication];
        void* module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        if (!module) {
            std::cerr << dlerror() << '\n';
            return 1;
        }
        Class                       cls    = (__bridge Class) dlsym(module, "OBJC_CLASS_$_BambuPlayer");
        AVSampleBufferDisplayLayer* layer  = [AVSampleBufferDisplayLayer layer];
        BambuPlayer*                player = [[cls alloc] initWithDisplayLayer:layer];
        if (!player)
            return 1;
        std::atomic<bool> disconnected{false};
        [player setLogger:[](const void* ctx, int level, const char*) {
            if (level < 0)
                const_cast<std::atomic<bool>*>(static_cast<const std::atomic<bool>*>(ctx))->store(true);
        }
              withContext:&disconnected];
        for (int run = 0; run < 2; ++run) {
            if ([player open:argv[2]] != 0 || [player play] != 0)
                return 1;
            NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:8];
            while (!layer.readyForDisplay && deadline.timeIntervalSinceNow > 0)
                [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
            NSSize size     = [player videoSize];
            bool   rendered = layer.readyForDisplay && size.width == 320 && size.height == 240;
            if (argc == 4 && run == 0) {
                while (!disconnected && deadline.timeIntervalSinceNow > 0)
                    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
                if (!disconnected) {
                    std::cerr << "Expected media disconnect after server closed the stream\n";
                    return 1;
                }
                std::cout << "PASS native player reported stream disconnect\n";
            }
            [player close];
            [layer flushAndRemoveImage];
            if (!rendered) {
                std::cerr << "Expected a rendered 320x240 frame\n";
                return 1;
            }
        }
        std::cout << "PASS rendered 320x240 camera stream and reopened it\n";
    }
}
