#include "libslic3r/Utils.hpp"
#include "slic3r/Utils/BBLNetworkPlugin.hpp"
#include "slic3r/Utils/BBLPrinterAgent.hpp"
#include "slic3r/Utils/BBLCloudServiceAgent.hpp"
#include "slic3r/GUI/MacBambuConnect.hpp"
#include <nlohmann/json.hpp>
#include <boost/filesystem.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace Slic3r;
using nlohmann::json;
namespace filesystem = std::filesystem;

static void check(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}

void verify_file_transfer_results();
void verify_printer_files(const filesystem::path& input, const filesystem::path& destination);

int main(int argc, char** argv)
{
    if (argc != 5) {
        std::cerr << "Usage: native_workflow NETWORK_FIXTURE SOURCE_FIXTURE SLICED_FILE RESOURCES\n";
        return 2;
    }
    auto dir = filesystem::temp_directory_path() / ("orcafix-" + boost::filesystem::unique_path().string());
    try {
        verify_file_transfer_results();
        filesystem::create_directories(dir / "plugins");
        filesystem::copy_file(argv[1], dir / "plugins/libbambu_networking.dylib");
        auto input = dir / u8"Spool #2 & café.gcode.3mf";
        filesystem::copy_file(argv[2], dir / "plugins/libBambuSource.dylib");
        filesystem::copy_file(argv[3], input);
        set_resources_dir(argv[4]);
        set_data_dir(dir.string());
        auto& plugin = BBLNetworkPlugin::instance();
        check(plugin.initialize(false, BAMBU_NETWORK_AGENT_VERSION) == 0, "load fixture");
        check(plugin.create_agent(dir.string()) != nullptr, "create agent");
        BBLCloudServiceAgent cloud;
        bool                 login_event = false;
        check(!cloud.is_user_login(), "signed out");
        cloud.set_on_user_login_fn([&](int, bool logged_in) { login_event = logged_in; });
        check(cloud.change_user("fixture-account") == 0 && cloud.is_user_login() && login_event, "login callback");
        unsigned int http_code = 0;
        std::string  account;
        check(cloud.get_user_print_info(&http_code, &account) == 0 && http_code == 200 && json::parse(account).at("devices").empty(),
              "empty account");
        BBLPrinterAgent printer;
        std::string     id;
        json            status;
        printer.set_on_ssdp_msg_fn([&](std::string record) { id = json::parse(record).at("dev_id"); });
        printer.set_on_local_message_fn([&](std::string device, std::string record) {
            check(device == id, "device callback");
            status = json::parse(record).at("print");
        });
        check(printer.start_discovery(true, false) && id == "fixture", "discover");
        auto connect = [&] { return printer.connect_printer(id, "127.0.0.1", "test", "", true); };
        check(connect() == 0 && status.at("gcode_state") == "IDLE", "connect");
        PrintParams params{};
        params.dev_id       = id;
        params.filename     = input.string();
        params.task_use_ams = true;
        params.ams_mapping  = "[2]";
        int  progress       = 0;
        auto update         = [&](int stage, int percent, std::string) {
            if (stage == PrintingStageUpload)
                progress = percent;
        };
        check(printer.start_local_print(params, update, [] { return true; }) == BAMBU_NETWORK_ERR_CANCELED, "cancel before upload");
        check(!filesystem::exists(dir / "received.gcode.3mf"), "cancelled upload wrote a file");
        check(printer.start_local_print(params, update, [] { return false; }) == 0 && progress == 100, "upload");
        auto bytes = [](const filesystem::path& file) {
            std::ifstream stream(file, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(stream), {});
        };
        check(bytes(input) == bytes(dir / "received.gcode.3mf"), "uploaded bytes");
        check(json::parse(bytes(dir / "ams.json")).at("mapping") == "[2]", "AMS slot");
        check(printer.disconnect_printer() == 0 && connect() == 0, "reconnect");
        check(!printer.retry_last_print_request(id), "acknowledged job replayed");
        check(status.at("job_id") == "1" && status.at("gcode_state") == "RUNNING", "resume existing job");
        for (auto [command, state] : {std::pair{"pause", "PAUSE"}, {"resume", "RUNNING"}, {"stop", "FAILED"}}) {
            auto request = json{{"print", {{"command", command}}}}.dump();
            check(printer.send_message_to_printer(id, request, 0, 0) == 0 && status.at("gcode_state") == state, command);
        }
        params.task_use_ams = false;
        params.ams_mapping.clear();
        check(printer.start_local_print(params, update, [] { return false; }) == 0, "external spool");
        check(!json::parse(bytes(dir / "ams.json")).at("use_ams").get<bool>(), "external spool mapping");
        check(printer.send_message_to_printer(id, R"({"print":{"command":"stop"}})", 0, 0) == 0, "stop external spool job");
        bool acknowledged = false;
        check(printer.start_print(
                  params, update, [] { return false; },
                  [&](int, std::string info) {
                      acknowledged = json::parse(info).at("job_id") == "3";
                      return acknowledged;
                  }) == 0 &&
                  acknowledged,
              "simulated cloud submission");
        check(cloud.user_logout(false) == 0 && !cloud.is_user_login() && !login_event, "logout callback");
        auto url = GUI::bambu_connect_import_url(input.string());
        check(url.find("bambu-connect://import-file?path=%2F") == 0, "Connect scheme");
        check(url.find("&name=Spool%20%232%20%26%20caf%C3%A9.gcode.3mf&version=1.0.0") != std::string::npos, "Connect filename");
        check(GUI::bambu_connect_import_url("relative.3mf").empty(), "relative path");
        verify_printer_files(input, dir / "downloads");
        plugin.destroy_agent();
        plugin.unload();
        filesystem::remove_all(dir);
        std::cout << "PASS simulated account, LAN and cloud submissions, bytes, controls, reconnect, AMS, file results and Connect URL\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\nTest directory: " << dir << '\n';
        return 1;
    }
}
