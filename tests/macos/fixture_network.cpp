// Native test plug-in. All device state and uploaded files belong to the test process.
#include "slic3r/Utils/bambu_networking.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

using namespace Slic3r;
using nlohmann::json;
namespace fs = std::filesystem;

namespace {
struct Printer
{
    fs::path       storage;
    OnMsgArrivedFn discovered;
    OnMessageFn    message;
    OnUserLoginFn  login_callback;
    bool           logged_in = false;
    std::string    selected;
    std::string    state       = "IDLE";
    unsigned       submissions = 0;

    void report() const
    {
        if (message)
            message(selected,
                    json{{"print", {{"command", "push_status"}, {"gcode_state", state}, {"job_id", std::to_string(submissions)}}}}.dump());
    }
};
} // namespace

extern "C" {
std::string bambu_network_get_version() { return BAMBU_NETWORK_AGENT_VERSION; }
void*       bambu_network_create_agent(std::string dir) { return new Printer{fs::path(dir)}; }
int         bambu_network_destroy_agent(void* handle)
{
    delete static_cast<Printer*>(handle);
    return 0;
}
int bambu_network_set_on_ssdp_msg_fn(void* handle, OnMsgArrivedFn callback)
{
    static_cast<Printer*>(handle)->discovered = std::move(callback);
    return 0;
}
int bambu_network_set_on_local_message_fn(void* handle, OnMessageFn callback)
{
    static_cast<Printer*>(handle)->message = std::move(callback);
    return 0;
}

int bambu_network_set_on_user_login_fn(void* handle, OnUserLoginFn cb)
{
    static_cast<Printer*>(handle)->login_callback = std::move(cb);
    return 0;
}
bool bambu_network_is_user_login(void* handle) { return static_cast<Printer*>(handle)->logged_in; }
int  bambu_network_change_user(void* handle, std::string user)
{
    auto* p      = static_cast<Printer*>(handle);
    p->logged_in = (user == "fixture-account");
    if (p->login_callback)
        p->login_callback(0, p->logged_in);
    return 0;
}
int bambu_network_user_logout(void* handle, bool) { return bambu_network_change_user(handle, {}); }
int bambu_network_get_user_print_info(void*, unsigned int* status, std::string* body)
{
    *status = 200;
    *body   = R"({"devices":[]})";
    return 0;
}

bool bambu_network_start_discovery(void* handle, bool start, bool)
{
    auto* p = static_cast<Printer*>(handle);
    if (start && p->discovered)
        p->discovered(
            R"({"dev_name":"Simulated P1S","dev_id":"fixture","dev_ip":"127.0.0.1","dev_type":"C12","dev_signal":"-42","connect_type":"lan","bind_state":"free"})");
    return true;
}

int bambu_network_connect_printer(void* handle, std::string id, std::string ip, std::string, std::string, bool)
{
    auto* p = static_cast<Printer*>(handle);
    if (id != "fixture" || ip != "127.0.0.1")
        return BAMBU_NETWORK_ERR_CONNECT_FAILED;
    p->selected = id;
    p->report();
    return 0;
}

int bambu_network_disconnect_printer(void* handle)
{
    static_cast<Printer*>(handle)->selected.clear();
    return 0;
}

int bambu_network_start_local_print(void* handle, PrintParams params, OnUpdateStatusFn update, WasCancelledFn cancel)
{
    auto* p = static_cast<Printer*>(handle);
    if (p->selected.empty() || params.dev_id != p->selected || p->state == "RUNNING")
        return BAMBU_NETWORK_ERR_INVALID_RESULT;
    if (cancel && cancel())
        return BAMBU_NETWORK_ERR_CANCELED;
    std::error_code error;
    fs::copy_file(params.filename, p->storage / "received.gcode.3mf", fs::copy_options::overwrite_existing, error);
    if (error)
        return BAMBU_NETWORK_ERR_OPEN_FILE_FAILED;
    std::ofstream mapping(p->storage / "ams.json");
    mapping << json{{"use_ams", params.task_use_ams}, {"mapping", params.ams_mapping}};
    ++p->submissions;
    p->state = "RUNNING";
    if (update)
        update(PrintingStageUpload, 100, "");
    p->report();
    return 0;
}

int bambu_network_start_print(void* handle, PrintParams params, OnUpdateStatusFn update, WasCancelledFn cancel, OnWaitFn wait)
{
    if (!static_cast<Printer*>(handle)->logged_in)
        return BAMBU_NETWORK_ERR_CONNECT_FAILED;
    int result = bambu_network_start_local_print(handle, params, update, cancel);
    if (result == 0 && wait)
        wait(0, json{{"job_id", std::to_string(static_cast<Printer*>(handle)->submissions)}}.dump());
    return result;
}

int bambu_network_send_message_to_printer(void* handle, std::string id, std::string payload, int, int)
{
    auto* p = static_cast<Printer*>(handle);
    if (p->selected.empty() || id != p->selected)
        return BAMBU_NETWORK_ERR_CONNECT_FAILED;
    const auto body = json::parse(payload, nullptr, false);
    if (!body.contains("print") || !body["print"].is_object())
        return BAMBU_NETWORK_ERR_INVALID_RESULT;
    const auto command = body["print"].value("command", std::string());
    if (command == "pause" && p->state == "RUNNING")
        p->state = "PAUSE";
    else if (command == "resume" && p->state == "PAUSE")
        p->state = "RUNNING";
    else if (command == "stop" && (p->state == "PAUSE" || p->state == "RUNNING"))
        p->state = "FAILED";
    else
        return BAMBU_NETWORK_ERR_INVALID_RESULT;
    p->report();
    return 0;
}
}
