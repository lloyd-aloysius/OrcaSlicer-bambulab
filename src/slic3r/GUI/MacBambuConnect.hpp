#pragma once

#include <string>

namespace Slic3r::GUI {
std::string bambu_connect_import_url(const std::string& path);
bool        open_in_bambu_connect(const std::string& path);
} // namespace Slic3r::GUI
