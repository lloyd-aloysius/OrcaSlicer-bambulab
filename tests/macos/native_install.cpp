#include "slic3r/GUI/MacPluginInstall.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
using Slic3r::GUI::install_native_mac_plugin;

int main(int argc, char** argv)
{
    if (argc != 6) {
        std::cerr << "Usage: native_install PLUGIN_ZIP EXTRACTED_PLUGIN VERIFIER EMPTY_TEST_PROFILE INCOMPATIBLE_LIBRARY\n";
        return 2;
    }
    fs::path    profile = argv[4], active = profile / "plugins";
    std::string error;
    auto        require = [](bool value, const char* description) {
        if (!value)
            throw std::runtime_error(description);
    };
    auto activate = [&](const fs::path& source, bool cancel = false) {
        return install_native_mac_plugin(source.string(), active.string(), argv[3], [=] { return cancel; }, error);
    };
    try {
        require(!fs::exists(active), "Expected an empty test profile");
        require(activate(argv[1]), "First install");
        std::ofstream(active / "generation") << "previous";
        std::ofstream(profile / "profile.conf") << "preserve";
        require(!activate(argv[1], true) && fs::exists(active / "generation"), "Interrupted install changed active libraries");
        fs::create_directory(profile / "incomplete");
        require(!activate(profile / "incomplete") && fs::exists(active / "generation"), "Incomplete update changed active libraries");
        auto incompatible = profile / "incompatible";
        fs::copy(argv[2], incompatible, fs::copy_options::recursive | fs::copy_options::copy_symlinks);
        fs::copy_file(argv[5], incompatible / "libbambu_networking.dylib", fs::copy_options::overwrite_existing);
        require(!activate(incompatible) && error.find("Incompatible plug-in version") != std::string::npos &&
                    fs::exists(active / "generation"),
                "Incompatible update changed active plug-in");
        require(activate(argv[2]), "OTA directory install");
        require(!fs::exists(active / "generation") && fs::exists(profile / "plugins-previous/generation"), "Previous version retention");
        fs::remove(active / "libBambuSource.dylib");
        require(activate(argv[1]) && fs::exists(profile / "plugins-previous/generation"), "Repair overwrote last working backup");
        require(fs::exists(profile / "profile.conf"), "User profile disappeared");
        std::cout << "PASS initial install, cancelled, incomplete and incompatible updates, upgrade and previous-version retention\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << ": " << error << '\n';
        return 1;
    }
}
