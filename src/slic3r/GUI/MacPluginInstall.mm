#import <Foundation/Foundation.h>
#include "MacPluginInstall.hpp"
#include <cstdio>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <filesystem>
#include <system_error>

namespace Slic3r::GUI {
namespace fs = std::filesystem;

static void run_tool(NSString* executable, NSArray<NSString*>* arguments)
{
    NSTask* task        = [[[NSTask alloc] init] autorelease];
    task.executableURL  = [NSURL fileURLWithPath:executable];
    task.arguments      = arguments;
    NSPipe* diagnostics = [NSPipe pipe];
    task.standardError  = diagnostics;
    NSError* error      = nil;
    if (![task launchAndReturnError:&error])
        throw std::runtime_error(error.localizedDescription.UTF8String);
    NSData* data = [diagnostics.fileHandleForReading readDataToEndOfFile];
    [task waitUntilExit];
    if (task.terminationStatus != 0) {
        NSString* reason = [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] autorelease];
        throw std::runtime_error(std::string(executable.lastPathComponent.UTF8String) + ": " + (reason.UTF8String ?: "failed"));
    }
}

bool install_native_mac_plugin(const std::string&           source,
                               const std::string&           active,
                               const std::string&           verifier,
                               const std::function<bool()>& cancelled,
                               std::string&                 error)
{
    @autoreleasepool {
        error.clear();
        fs::path staging;
        struct Cleanup
        {
            fs::path& path;
            ~Cleanup()
            {
                std::error_code ec;
                if (!path.empty())
                    fs::remove_all(path, ec);
            }
        } cleanup{staging};
        try {
            const fs::path parent = fs::path(active).parent_path();
            fs::create_directories(parent);
            std::string pattern = (parent / ".native-install-XXXXXX").string();
            if (!mkdtemp(pattern.data()))
                throw std::system_error(errno, std::generic_category(), "Create staging directory");
            staging             = pattern;
            fs::path  candidate = staging / "plugins";
            NSString* input     = [NSString stringWithUTF8String:source.c_str()];
            NSString* output    = [NSString stringWithUTF8String:candidate.c_str()];
            if (fs::is_directory(source))
                run_tool(@"/usr/bin/ditto", @[input, output]);
            else
                run_tool(@"/usr/bin/ditto", @[@"-x", @"-k", input, output]);
            if (cancelled && cancelled())
                return false;
            run_tool([NSString stringWithUTF8String:verifier.c_str()], @[output]);
            if (cancelled && cancelled())
                return false;

            // Exchange complete directories on the same volume. A crash leaves a complete active set.
            struct Lock
            {
                int fd;
                ~Lock()
                {
                    if (fd >= 0)
                        close(fd);
                }
            } lock{open((parent / ".native-plugin.lock").c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600)};
            if (lock.fd < 0 || flock(lock.fd, LOCK_EX) != 0)
                throw std::system_error(errno, std::generic_category(), "Lock plug-in installation");
            if (cancelled && cancelled())
                return false;
            fs::create_directories(active);
            bool retain = fs::exists(fs::path(active) / "libbambu_networking.dylib");
            if (retain) {
                // A repair of damaged active files must preserve the last working backup.
                try {
                    run_tool([NSString stringWithUTF8String:verifier.c_str()], @[[NSString stringWithUTF8String:active.c_str()]]);
                } catch (const std::exception&) {
                    retain = false;
                }
            }
            const fs::path previous = parent / "plugins-previous";
            if (retain)
                fs::create_directories(previous);
            if (renameatx_np(AT_FDCWD, candidate.c_str(), AT_FDCWD, active.c_str(), RENAME_SWAP) != 0)
                throw std::system_error(errno, std::generic_category(), "Activate native plug-in");
            if (retain && renameatx_np(AT_FDCWD, candidate.c_str(), AT_FDCWD, previous.c_str(), RENAME_SWAP) != 0) {
                error = "Previous plug-in preserved in " + candidate.string();
                staging.clear();
            }
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
}
} // namespace Slic3r::GUI
