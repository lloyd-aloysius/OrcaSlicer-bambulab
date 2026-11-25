// Check the staged native libraries in a separate process before activating an update.
#include "slic3r/Utils/bambu_networking.hpp"

#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>

template<class Function> Function symbol(void* library, const char* name)
{
    void* result = dlsym(library, name);
    if (!result)
        throw std::runtime_error(name);
    return reinterpret_cast<Function>(result);
}

int main(int argc, char** argv)
{
    if (argc != 2 && argc != 4) {
        std::cerr << "Usage: orca-native-plugin-check PLUGIN_DIRECTORY [LOG_DIRECTORY CERT_DIRECTORY]\n";
        return 2;
    }
    const std::filesystem::path plugins = argv[1];
    try {
        void* network = dlopen((plugins / "libbambu_networking.dylib").c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!network)
            throw std::runtime_error(dlerror());
        void* source = dlopen((plugins / "libBambuSource.dylib").c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!source)
            throw std::runtime_error(dlerror());
        symbol<void*>(source, "OBJC_CLASS_$_BambuPlayer");
        symbol<void*>(source, "Bambu_ReadSample");
        auto version   = symbol<std::string (*)()>(network, "bambu_network_get_version")();
        auto create    = symbol<void* (*) (std::string)>(network, "bambu_network_create_agent");
        auto destroy   = symbol<int (*)(void*)>(network, "bambu_network_destroy_agent");
        auto logged_in = symbol<bool (*)(void*)>(network, "bambu_network_is_user_login");
        // Match the three-part ABI family accepted by GUI_App::check_networking_version.
        if (version.size() < 8 || version.compare(0, 8, std::string(BAMBU_NETWORK_AGENT_VERSION).substr(0, 8)) != 0)
            throw std::runtime_error("Incompatible plug-in version: " + version);
        symbol<void*>(network, "ft_tunnel_create");
        symbol<void*>(network, "ft_job_create");
        if (argc == 2) {
            std::cout << "Compatible native plug-in " << version << '\n';
            return 0;
        }
        void* agent = create(argv[2]);
        if (!agent)
            throw std::runtime_error("Failed to create network agent");
        auto config   = symbol<int (*)(void*, std::string)>(network, "bambu_network_set_config_dir");
        auto start    = symbol<int (*)(void*)>(network, "bambu_network_start");
        auto init_log = symbol<int (*)(void*)>(network, "bambu_network_init_log");
        auto cert     = symbol<int (*)(void*, std::string, std::string)>(network, "bambu_network_set_cert_file");
        auto country  = symbol<int (*)(void*, std::string)>(network, "bambu_network_set_country_code");
        if (config(agent, std::filesystem::absolute(argv[2]).string()) || init_log(agent) ||
            cert(agent, std::filesystem::absolute(argv[3]).string(), "slicer_base64.cer") || country(agent, "GB") || start(agent))
            throw std::runtime_error("Agent initialization failed");
        const bool session = logged_in(agent);
        if (destroy(agent) != 0 || session)
            throw std::runtime_error("Agent cleanup or empty-session check failed");
        std::cout << "PASS native network " << version << ", camera exports, signed-out agent lifecycle\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
