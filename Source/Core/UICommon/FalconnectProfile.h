#pragma once

#include <optional>
#include <string>

namespace UICommon
{
struct DolphinProfilePaths
{
  std::string user;
  std::string config;
};

enum class ProfileImportResult
{
  NoSource,
  AlreadyImported,
  Imported,
  Failed
};

enum class ProfileImportKind { Settings, Saves, All };
std::optional<DolphinProfilePaths> FindDolphinProfile();
ProfileImportResult ImportDolphinProfile(const DolphinProfilePaths& source,
                                       const DolphinProfilePaths& destination,
                                       ProfileImportKind kind = ProfileImportKind::All,
                                       bool overwrite = false);
ProfileImportResult ImportInstalledDolphinProfile();
// Imports a validated NTSC GX GCI/GCS/SAV into an isolated GCI folder.
bool ImportGXSave(const std::string& filename, const std::string& destination_folder);
bool ImportGXSaveToProfile(const std::string& filename, const DolphinProfilePaths& destination);
}  // namespace UICommon
