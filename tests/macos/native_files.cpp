#include <wx/app.h>
#include <wx/log.h>
#include <nlohmann/json.hpp>
#include "slic3r/GUI/Printer/PrinterFileSystem.h"
#include <boost/make_shared.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace filesystem = std::filesystem;

void verify_printer_files(const filesystem::path& input, const filesystem::path& destination)
{
    wxApp::SetInstance(new wxApp);
    int    argc = 0;
    char** argv = nullptr;
    if (!wxEntryStart(argc, argv))
        throw std::runtime_error("wxWidgets startup failed");
    wxLog::SetActiveTarget(new wxLogStderr);
    auto files     = boost::make_shared<PrinterFileSystem>();
    bool connected = false, uploaded = false, downloaded = false;
    bool delete_refused = false;
    files->Bind(EVT_FILE_CHANGED, [&](wxCommandEvent& e) {
        if (e.GetExtraLong() == PrinterFileSystem::FILE_READ_WRITE_ERR)
            delete_refused = true;
    });
    files->Bind(EVT_STATUS_CHANGED, [&](wxCommandEvent& e) {
        if (e.GetInt() == PrinterFileSystem::Initializing)
            files->SetUrl("fixture://storage");
        if (e.GetInt() == PrinterFileSystem::ListSyncing)
            connected = true;
    });
    files->Bind(EVT_UPLOAD_CHANGED, [&](wxCommandEvent& e) { uploaded = e.GetInt() == PrinterFileSystem::FF_UPLOADDONE; });
    files->Bind(EVT_DOWNLOAD, [&](wxCommandEvent& e) { downloaded = e.GetString() == (destination / input.filename()).string(); });
    auto until = [&](auto done, const char* step) {
        auto end = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        while (!done()) {
            wxTheApp->ProcessPendingEvents();
            if (std::chrono::steady_clock::now() > end)
                throw std::runtime_error(step);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    };
    files->SetFileType(PrinterFileSystem::F_MODEL, "sdcard");
    files->Attached();
    files->Start();
    until([&] { return connected; }, "storage connection");
    files->RequestMediaAbility(2);
    until([&] { return !files->GetMediaAbilityList().empty(); }, "storage capabilities");
    files->SetUploadFile(input.string(), input.filename().string(), "sdcard");
    files->RequestUploadFile();
    until([&] { return uploaded; }, "storage upload");
    files->ListAllFiles();
    until([&] { return files->GetCount() == 1; }, "list uploaded file");
    files->DownloadFiles(0, destination.string());
    until([&] { return downloaded; }, "file download");
    std::ifstream source(input, std::ios::binary), copy(destination / input.filename(), std::ios::binary);
    if (!copy || !std::equal(std::istreambuf_iterator<char>(source), {}, std::istreambuf_iterator<char>(copy), {}))
        throw std::runtime_error("Downloaded bytes differ");
    files->DeleteFiles(0);
    until([&] { return delete_refused; }, "file deletion error");
    if (files->GetCount() != 1)
        throw std::runtime_error("Failed deletion removed file from listing");
    files->DeleteFiles(0);
    until([&] { return files->GetCount() == 0; }, "delete file");
    files->Stop(true);
    until([&] { return files.unique(); }, "file worker shutdown");
    wxTheApp->ProcessPendingEvents();
    files.reset();
    wxEntryCleanup();
}
