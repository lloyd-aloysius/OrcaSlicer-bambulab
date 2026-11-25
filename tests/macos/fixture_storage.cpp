// In-memory source module for the Device tab's file browser.
#include "slic3r/GUI/Printer/BambuTunnel.h"
#include <nlohmann/json.hpp>
#include <deque>
#include <string>

using nlohmann::json;
namespace {
struct Storage
{
    std::string             name, contents, checksum, sample;
    std::deque<std::string> replies;
    bool                    reject_first_delete = true;

    void reply(int seq, int code, const json& body, const std::string& bytes = {})
    {
        replies.push_back(json{{"sequence", seq}, {"result", code}, {"reply", body}}.dump() + "\n\n" + bytes);
    }
};
} // namespace
extern "C" {
int Bambu_Create(Bambu_Tunnel* tunnel, char const* url)
{
    if (std::string(url) != "fixture://storage")
        return 1;
    *tunnel = new Storage;
    return 0;
}
void Bambu_SetLogger(Bambu_Tunnel, Logger, void*) {}
int  Bambu_Open(Bambu_Tunnel) { return 0; }
int  Bambu_StartStreamEx(Bambu_Tunnel, int) { return 0; }
void Bambu_Close(Bambu_Tunnel) {}
void Bambu_Destroy(Bambu_Tunnel tunnel) { delete static_cast<Storage*>(tunnel); }

int Bambu_SendMessage(Bambu_Tunnel tunnel, int, const char* data, int len)
{
    auto&       s = *static_cast<Storage*>(tunnel);
    std::string msg(data, len);
    auto        sep  = msg.find("\n\n");
    auto        root = json::parse(msg.substr(0, sep));
    const auto& req  = root.at("req");
    const int   seq  = root.at("sequence");
    switch (root.at("cmdtype").get<int>()) {
    case 7: s.reply(seq, 0, {{"storage", {"sdcard"}}}); break;
    case 1: {
        json entries = json::array();
        if (!s.checksum.empty())
            entries.push_back({{"name", s.name}, {"path", s.name}, {"size", s.contents.size()}, {"time", 1700000000}});
        s.reply(seq, 0, {{"file_lists", entries}});
        break;
    }
    case 5:
        if (sep == std::string::npos) {
            s.name = req.at("path");
            s.contents.clear();
            s.checksum.clear();
            s.reply(seq, 1, {{"chunk_size", 32768}, {"offset", 0}});
        } else if (req.at("offset") == s.contents.size() && req.at("size") == msg.size() - sep - 2) {
            s.contents.append(msg, sep + 2);
            if (req.contains("file_md5")) {
                s.checksum = req.at("file_md5");
                s.reply(seq, 0, {});
            }
        } else {
            s.reply(seq, 12, {});
        }
        break;
    case 4:
        if (req.at("path") == s.name && !s.checksum.empty())
            s.reply(seq, 0, {{"offset", 0}, {"size", s.contents.size()}, {"total", s.contents.size()}, {"file_md5", s.checksum}},
                    s.contents);
        else
            s.reply(seq, 10, {});
        break;
    case 3:
        if (s.reject_first_delete) {
            s.reject_first_delete = false;
            s.reply(seq, 14, {});
            break;
        }
        if (req.at("paths").at(0) == s.name) {
            s.checksum.clear();
            s.contents.clear();
            s.reply(seq, 0, {});
        } else
            s.reply(seq, 10, {});
        break;
    default: s.reply(seq, 18, {});
    }
    return 0;
}
int Bambu_ReadSample(Bambu_Tunnel tunnel, Bambu_Sample* sample)
{
    auto& s = *static_cast<Storage*>(tunnel);
    if (s.replies.empty())
        return Bambu_would_block;
    s.sample = std::move(s.replies.front());
    s.replies.pop_front();
    *sample        = {};
    sample->buffer = reinterpret_cast<const unsigned char*>(s.sample.data());
    sample->size   = static_cast<int>(s.sample.size());
    return 0;
}
}
