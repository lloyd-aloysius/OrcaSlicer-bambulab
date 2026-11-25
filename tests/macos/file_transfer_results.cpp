#include "slic3r/Utils/FileTransferUtils.hpp"
#include <stdexcept>

using namespace Slic3r;

namespace {
struct Job
{
    void (*notify)(void*, ft_job_result){};
    void* context{};
    bool  ready{};
};
int            releases  = 0;
constexpr char payload[] = "3mf";

ft_job_result response() { return {FT_EIO, 42, R"({"storage":"sdcard"})", payload, 3}; }
} // namespace

void verify_file_transfer_results()
{
    FileTransferModule api(nullptr);
    api.ft_job_create = [](const char*, FT_JobHandle** out) {
        *out = reinterpret_cast<FT_JobHandle*>(new Job);
        return FT_OK;
    };
    api.ft_job_release       = [](FT_JobHandle* job) { delete reinterpret_cast<Job*>(job); };
    api.ft_job_set_result_cb = [](FT_JobHandle* handle, void (*cb)(void*, ft_job_result), void* ctx) {
        auto* job    = reinterpret_cast<Job*>(handle);
        job->notify  = cb;
        job->context = ctx;
        return FT_OK;
    };
    api.ft_job_get_result = [](FT_JobHandle* handle, uint32_t, ft_job_result* out) {
        if (!reinterpret_cast<Job*>(handle)->ready)
            return FT_ETIMEOUT;
        *out = response();
        return FT_OK;
    };
    api.ft_job_result_destroy = [](ft_job_result*) { ++releases; };

    FileTransferJob        transfer(api, R"({"cmd_type":7})");
    int                    ec = 0, device_ec = 0;
    std::string            json;
    std::vector<std::byte> binary;
    if (transfer.get_result(ec, device_ec, json, binary, 1) || releases != 0)
        throw std::runtime_error("Timed-out file job consumed a result");
    auto& job      = *reinterpret_cast<Job*>(transfer.native());
    bool  observed = false;
    transfer.on_result([&](int status, int device, std::string text, std::vector<std::byte> bytes) {
        observed = status == FT_EIO && device == 42 && text == response().json && bytes.size() == 3;
    });
    job.ready = true;
    job.notify(job.context, response());
    if (!transfer.finished() || !observed || releases != 1)
        throw std::runtime_error("Asynchronous file result or ownership mismatch");
    if (!transfer.get_result(ec, device_ec, json, binary, 1) || ec != FT_EIO || device_ec != 42 || json != response().json ||
        binary.size() != 3 || std::memcmp(binary.data(), payload, 3) || releases != 2)
        throw std::runtime_error("Synchronous file result or ownership mismatch");
}
