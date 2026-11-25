#pragma once
#include <functional>
#include <string>

namespace Slic3r::GUI {
// source can be a downloaded zip or an extracted OTA directory.
bool install_native_mac_plugin(const std::string&           source,
                               const std::string&           active,
                               const std::string&           verifier,
                               const std::function<bool()>& cancelled,
                               std::string&                 error);
} // namespace Slic3r::GUI
