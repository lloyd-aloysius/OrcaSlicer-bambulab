#import <AppKit/AppKit.h>

#include "MacBambuConnect.hpp"

namespace Slic3r::GUI {

std::string bambu_connect_import_url(const std::string& path)
{
    @autoreleasepool {
        NSString* filename = [NSString stringWithUTF8String:path.c_str()];
        if (![filename isAbsolutePath])
            return {};
        NSCharacterSet* allowed      = [NSCharacterSet
            characterSetWithCharactersInString:@"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.!~*'()"];
        NSString*       encoded_path = [filename stringByAddingPercentEncodingWithAllowedCharacters:allowed];
        NSString*       name         = [filename.lastPathComponent stringByAddingPercentEncodingWithAllowedCharacters:allowed];
        NSString*       url = [NSString stringWithFormat:@"bambu-connect://import-file?path=%@&name=%@&version=1.0.0", encoded_path, name];
        return url.UTF8String;
    }
}

bool open_in_bambu_connect(const std::string& path)
{
    @autoreleasepool {
        const auto import_url = bambu_connect_import_url(path);
        if (import_url.empty())
            return false;
        NSURL* url = [NSURL URLWithString:[NSString stringWithUTF8String:import_url.c_str()]];
        return url && [[NSWorkspace sharedWorkspace] URLForApplicationToOpenURL:url] && [[NSWorkspace sharedWorkspace] openURL:url];
    }
}
} // namespace Slic3r::GUI
