#include "UICommon/FalconnectProfile.h"

#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <string_view>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <wil/resource.h>
#endif

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Core/Config/MainSettings.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/GCMemcard/GCMemcardUtils.h"
#include "DiscIO/Enums.h"

namespace UICommon
{
namespace
{
namespace fs = std::filesystem;
std::string Lower(std::string value)
{
  Common::ToLower(&value);
  return value;
}

bool SaveSetting(std::string_view key)
{
  return key == "memcardapath" || key == "memcardbpath" ||
         key.starts_with("gcifolder") || key == "slota" || key == "slotb" ||
         key == "memorycardsize";
}

constexpr char MARKER[] = "FalconnectDolphinImport-v1.txt";

std::string Environment(const char* key)
{
  const char* value = std::getenv(key);
  return value ? value : "";
}

fs::path Path(const std::string& value)
{
  return fs::path(std::u8string(value.begin(), value.end()));
}

std::string String(const fs::path& path)
{
  const auto value = path.u8string();
  return {value.begin(), value.end()};
}

// Neither source nor destination links may turn a copy into shared writable storage.
// Inspect ancestors too, so a linked directory cannot escape the destination profile.
bool IsLink(const fs::path& path)
{
  std::error_code ec;
  for (fs::path current = path; !current.empty() && current != current.root_path();
       current = current.parent_path())
  {
    if (fs::is_symlink(fs::symlink_status(current, ec)))
      return true;
    ec.clear();
  }
  return false;
}

bool Exists(const fs::path& path)
{
  std::error_code ec;
  return fs::exists(path, ec);
}

bool CopyMissing(const fs::path& source, const fs::path& destination, bool overwrite = false)
{
  if (IsLink(source) || IsLink(destination))
    return false;
  if (!Exists(source))
    return true;
  std::error_code ec;
  if (fs::is_directory(source, ec))
  {
    fs::create_directories(destination, ec);
    if (ec)
      return false;
    fs::directory_iterator it(source, ec), end;
    if (ec)
      return false;
    for (; it != end; it.increment(ec))
    {
      if (ec || !CopyMissing(it->path(), destination / it->path().filename(), overwrite))
        return false;
    }
    return !ec;
  }
  if (!fs::is_regular_file(source, ec) || ec)
    return false;
  if (Exists(destination) && !overwrite)
    return fs::is_regular_file(destination, ec) && !ec;
  if (!overwrite && Lower(source.extension().string()) == ".gci" &&
      Exists(destination.parent_path()))
  {
    const auto incoming = Memcard::ReadSavefile(String(source));
    if (const auto* save = std::get_if<Memcard::Savefile>(&incoming))
    {
      fs::directory_iterator it(destination.parent_path(), ec), end;
      if (ec)
        return false;
      for (; it != end; it.increment(ec))
      {
        if (ec || IsLink(it->path()))
          return false;
        if (Lower(it->path().extension().string()) != ".gci")
          continue;
        const auto existing = Memcard::ReadSavefile(String(it->path()));
        const auto* old_save = std::get_if<Memcard::Savefile>(&existing);
        if (old_save && Memcard::HasSameIdentity(save->dir_entry, old_save->dir_entry))
          return true;
      }
      if (ec)
        return false;
    }
  }
  fs::create_directories(destination.parent_path(), ec);
  if (ec)
    return false;
  return fs::copy_file(source, destination,
                       overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::none,
                       ec) && !ec;
}

bool SafeSection(std::string_view section, bool dolphin_config)
{
  const std::string lower = Lower(std::string(section));
  if (lower.starts_with("actionreplay") || lower.starts_with("gecko") ||
      lower.starts_with("onframe"))
    return false;
  if (!dolphin_config)
    return true;
  return lower == "core" || lower == "general" || lower == "interface" || lower == "display" ||
         lower == "dsp" || lower == "gamelist" || lower == "input";
}

bool MergeSettings(const fs::path& source, const fs::path& destination,
                   const fs::path& destination_user, bool dolphin_config,
                   ProfileImportKind kind, bool overwrite)
{
  if (IsLink(source) || IsLink(destination))
    return false;
  Common::IniFile input, output;
  if ((Exists(source) && !input.Load(String(source))) ||
      (Exists(destination) && !output.Load(String(destination))))
    return false;
  if (overwrite && kind != ProfileImportKind::Settings)
  {
    std::vector<std::pair<std::string, std::string>> remove;
    for (const auto& section : output.GetSections())
      for (const auto& [key, value] : section.GetValues())
        if (SaveSetting(Lower(key)))
          remove.emplace_back(section.GetName(), key);
    for (const auto& [section, key] : remove)
      output.DeleteKey(section, key);
    if (dolphin_config)
    {
      auto* core = output.GetOrCreateSection("Core");
      core->Set("MemcardAPath", std::string{});
      core->Set("MemcardBPath", std::string{});
      core->Set("GCIFolderAPath", std::string{});
      core->Set("GCIFolderBPath", std::string{});
      core->Set("GCIFolderAPathOverride", std::string{});
      core->Set("GCIFolderBPathOverride", std::string{});
      core->Set("SlotA", static_cast<int>(ExpansionInterface::EXIDeviceType::MemoryCardFolder));
      core->Set("SlotB", static_cast<int>(ExpansionInterface::EXIDeviceType::None));
      core->Set("MemoryCardSize", -1);
    }
  }
  for (const auto& section : input.GetSections())
  {
    if (!SafeSection(section.GetName(), dolphin_config))
      continue;
    for (const auto& [key, original_value] : section.GetValues())
    {
      const bool save_setting = SaveSetting(Lower(key));
      if ((kind == ProfileImportKind::Settings && save_setting) ||
          (kind == ProfileImportKind::Saves && !save_setting))
        continue;
      if (!overwrite && output.Exists(section.GetName(), key))
        continue;
      std::string value = original_value;
      const std::string lower = Lower(key);
      // Never inherit writable paths into another profile. ISO search paths are read-only.
      if (lower.find("path") != std::string::npos || lower == "wiisdcardsyncfolder")
      {
        if (lower.starts_with("isopath"))
        {
          // Preserve game discovery without copying disc images.
        }
        else if (!value.empty() && (lower == "memcardapath" || lower == "memcardbpath"))
        {
          const fs::path original = Path(value);
          if (!original.is_absolute() || IsLink(original.parent_path()))
            continue;
          const auto folder = destination_user / "GC" / "Imported" / source.stem() / key;
          std::error_code ec;
          fs::directory_iterator it(original.parent_path(), ec), end;
          if (ec)
            return false;
          // Dolphin adds region and size suffixes, so retain the raw-card filenames.
          for (; it != end; it.increment(ec))
          {
            if (ec)
              return false;
            if ((it->path().extension() == ".raw" || it->path().extension() == ".gcp") &&
                !CopyMissing(it->path(), folder / it->path().filename(), overwrite))
              return false;
          }
          if (ec)
            return false;
          value = String(folder / original.filename());
        }
        else if (!value.empty() && (lower == "gcifolderapath" || lower == "gcifolderbpath"))
        {
          if (!Path(value).is_absolute())
            continue;
          const auto slot = lower == "gcifolderapath" ? ExpansionInterface::Slot::A :
                                                       ExpansionInterface::Slot::B;
          const auto original = Path(Config::GetGCIFolderPath(value, slot, DiscIO::Region::NTSC_U));
          const auto folder = destination_user / "GC" / "Imported" / source.stem() / key / "USA";
          if (!CopyMissing(original, folder, overwrite))
            return false;
          value = String(folder);
        }
        else
        {
          // Use Falconnect's own default dump/load/NAND/SD and other writable directories.
          continue;
        }
      }
      output.GetOrCreateSection(section.GetName())->Set(key, value);
    }
  }
  std::error_code ec;
  fs::create_directories(destination.parent_path(), ec);
  return !ec && output.Save(String(destination));
}

bool IsProfile(const DolphinProfilePaths& profile)
{
  return Exists(Path(profile.config) / "Dolphin.ini") || Exists(Path(profile.user) / "GC");
}
}  // namespace

std::optional<DolphinProfilePaths> FindDolphinProfile()
{
  std::vector<DolphinProfilePaths> candidates;
  const auto add = [&](const std::string& root) {
    if (!root.empty())
      candidates.push_back({root, String(Path(root) / "Config")});
  };
#ifdef _WIN32
  DWORD local = 0, size = sizeof(local);
  RegGetValueW(HKEY_CURRENT_USER, L"Software\\Dolphin Emulator", L"LocalUserConfig",
               RRF_RT_REG_DWORD, nullptr, &local, &size);
  if (local || Exists(Path(File::GetExeDirectory()) / "portable.txt"))
    add(String(Path(File::GetExeDirectory()) / "User"));
  wchar_t registry_path[32768]{};
  size = sizeof(registry_path);
  if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Dolphin Emulator", L"UserConfigPath",
                   RRF_RT_REG_SZ, nullptr, registry_path, &size) == ERROR_SUCCESS)
    add(WStringToUTF8(registry_path));
  wil::unique_cotaskmem_string documents, appdata;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, documents.put())))
    add(String(Path(WStringToUTF8(documents.get())) / "Dolphin Emulator"));
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, appdata.put())))
    add(String(Path(WStringToUTF8(appdata.get())) / "Dolphin Emulator"));
#else
  add(Environment("DOLPHIN_EMU_USERPATH"));
  const fs::path home = Path(Environment("HOME"));
#ifdef __APPLE__
  add(String(Path(File::GetExeDirectory()) / "User"));
  add(String(home / "Library/Application Support/Dolphin"));
#else
  add(String(Path(File::GetExeDirectory()) / "user"));
  add(String(home / ".dolphin-emu"));
  const auto xdg = [&](const char* key, const char* fallback) {
    const fs::path configured = Path(Environment(key));
    return configured.is_absolute() ? configured : home / fallback;
  };
  candidates.push_back({String(xdg("XDG_DATA_HOME", ".local/share") / "dolphin-emu"),
                        String(xdg("XDG_CONFIG_HOME", ".config") / "dolphin-emu")});
  candidates.push_back({String(home / ".var/app/org.DolphinEmu.dolphin-emu/data/dolphin-emu"),
                        String(home / ".var/app/org.DolphinEmu.dolphin-emu/config/dolphin-emu")});
#endif
#endif
  const auto it = std::ranges::find_if(candidates, IsProfile);
  return it == candidates.end() ? std::nullopt : std::optional{*it};
}

ProfileImportResult ImportDolphinProfile(const DolphinProfilePaths& source,
                                       const DolphinProfilePaths& destination,
                                       ProfileImportKind kind, bool overwrite)
{
  if (!IsProfile(source))
    return ProfileImportResult::NoSource;
  std::error_code ec;
  const fs::path user = fs::weakly_canonical(Path(destination.user), ec);
  if (ec)
    return ProfileImportResult::Failed;
  const fs::path config = fs::weakly_canonical(Path(destination.config), ec);
  if (ec)
    return ProfileImportResult::Failed;
  if (!overwrite && kind == ProfileImportKind::All && Exists(user / MARKER))
    return ProfileImportResult::AlreadyImported;
  const auto source_root = fs::weakly_canonical(Path(source.user), ec);
  if (ec)
    return ProfileImportResult::Failed;
  const auto source_config = fs::weakly_canonical(Path(source.config), ec);
  if (ec || source_root == user || source_config == config)
    return ProfileImportResult::Failed;
  const auto inside = [](const fs::path& child, const fs::path& parent) {
    auto relative = child.lexically_relative(parent);
    return !relative.empty() && *relative.begin() != "..";
  };
  if (inside(source_root, user) || inside(source_config, config) || IsLink(user / "GameSettings"))
    return ProfileImportResult::Failed;
  if (overwrite && kind != ProfileImportKind::Settings && Exists(user / "GC"))
  {
    if (IsLink(user / "GC"))
      return ProfileImportResult::Failed;
    const auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    const auto backup = user / ("GC.before-import-" + std::to_string(timestamp));
    // Move the old tree aside so different filenames for the same GCI identity cannot survive.
    // This also retains a recovery copy if an import fails part way through.
    if (Exists(backup))
      return ProfileImportResult::Failed;
    fs::rename(user / "GC", backup, ec);
    if (ec)
      return ProfileImportResult::Failed;
  }
  if (kind != ProfileImportKind::Settings && !CopyMissing(source_root / "GC", user / "GC", overwrite))
    return ProfileImportResult::Failed;
  if (kind != ProfileImportKind::Saves)
  {
    for (const char* folder : {"Load/Textures", "Shaders"})
      if (!CopyMissing(source_root / folder, user / folder, overwrite))
        return ProfileImportResult::Failed;
    Common::IniFile dolphin;
    dolphin.Load(String(source_config / "Dolphin.ini"));
    std::string custom_load;
    if (dolphin.GetIfExists("General", "LoadPath", &custom_load) &&
        Path(custom_load).is_absolute() &&
        !CopyMissing(Path(custom_load) / "Textures", user / "Load/Textures", overwrite))
      return ProfileImportResult::Failed;
    if (!CopyMissing(source_config / "Profiles", config / "Profiles", overwrite))
      return ProfileImportResult::Failed;
    for (const char* filename : {"GFX.ini", "GCPadNew.ini", "GCKeyNew.ini", "Hotkeys.ini"})
      if (!MergeSettings(source_config / filename, config / filename, user, false, kind, overwrite))
        return ProfileImportResult::Failed;
  }
  if (!MergeSettings(source_config / "Dolphin.ini", config / "Dolphin.ini", user, true, kind, overwrite))
    return ProfileImportResult::Failed;
  const auto games = source_root / "GameSettings";
  if (overwrite && kind == ProfileImportKind::Saves && Exists(user / "GameSettings"))
  {
    fs::directory_iterator it(user / "GameSettings", ec), end;
    if (ec)
      return ProfileImportResult::Failed;
    for (; it != end; it.increment(ec))
    {
      if (ec || IsLink(it->path()))
        return ProfileImportResult::Failed;
      if (it->path().extension() == ".ini" && !Exists(games / it->path().filename()) &&
          !MergeSettings(games / it->path().filename(), it->path(), user, false, kind, overwrite))
        return ProfileImportResult::Failed;
    }
  }
  if (IsLink(games))
    return ProfileImportResult::Failed;
  if (Exists(games))
  {
    fs::directory_iterator it(games, ec), end;
    if (ec)
      return ProfileImportResult::Failed;
    for (; it != end; it.increment(ec))
    {
      if (ec || IsLink(it->path()))
        return ProfileImportResult::Failed;
      if (it->path().extension() == ".ini" &&
          !MergeSettings(it->path(), user / "GameSettings" / it->path().filename(), user, false, kind, overwrite))
        return ProfileImportResult::Failed;
    }
    if (ec)
      return ProfileImportResult::Failed;
  }
  fs::create_directories(user, ec);
  if (kind != ProfileImportKind::All || overwrite)
    return ec ? ProfileImportResult::Failed : ProfileImportResult::Imported;
  if (ec || !File::WriteStringToFile(String(user / MARKER), source.user + "\n"))
    return ProfileImportResult::Failed;
  return ProfileImportResult::Imported;
}

ProfileImportResult ImportInstalledDolphinProfile()
{
  const auto source = FindDolphinProfile();
  return source ? ImportDolphinProfile(*source, {File::GetUserPath(D_USER_IDX),
                                                File::GetUserPath(D_CONFIG_IDX)}) :
                  ProfileImportResult::NoSource;
}
bool ImportGXSave(const std::string& filename, const std::string& destination_folder)
{
  auto result = Memcard::ReadSavefile(filename);
  const auto* save = std::get_if<Memcard::Savefile>(&result);
  if (!save || save->blocks.empty() || save->dir_entry.m_gamecode != std::array<u8, 4>{'G', 'F', 'Z', 'E'} ||
      save->dir_entry.m_makercode != std::array<u8, 2>{'0', '1'})
    return false;
  const fs::path folder = Path(destination_folder);
  if (IsLink(folder))
    return false;
  std::error_code ec;
  fs::create_directories(folder, ec);
  if (ec)
    return false;
  fs::path target = folder / (Memcard::GenerateFilename(save->dir_entry) + ".gci");
  fs::directory_iterator it(folder, ec), end;
  if (ec)
    return false;
  for (; it != end; it.increment(ec))
  {
    if (ec || IsLink(it->path()))
      return false;
    if (it->path().extension() != ".gci")
      continue;
    const auto existing = Memcard::ReadSavefile(String(it->path()));
    const auto* old_save = std::get_if<Memcard::Savefile>(&existing);
    if (old_save && Memcard::HasSameIdentity(old_save->dir_entry, save->dir_entry))
      target = it->path();
  }
  if (ec || IsLink(target))
    return false;
  const auto temporary = folder / "falconnect-import.tmp";
  if (Exists(temporary) || IsLink(temporary) ||
      !Memcard::WriteSavefile(String(temporary), *save, Memcard::SavefileFormat::GCI))
    return false;
  return File::Rename(String(temporary), String(target));
}
bool ImportGXSaveToProfile(const std::string& filename, const DolphinProfilePaths& destination)
{
  const fs::path user = Path(destination.user);
  const auto folder = user / "GC" / "ImportedGX" / "USA";
  const auto games = user / "GameSettings";
  std::vector<fs::path> settings = {Path(destination.config) / "Dolphin.ini", games / "GFZE01.ini"};
  std::error_code ec;
  if (IsLink(games) || IsLink(Path(destination.config)))
    return false;
  if (Exists(games))
  {
    fs::directory_iterator it(games, ec), end;
    if (ec)
      return false;
    for (; it != end; it.increment(ec))
    {
      if (ec)
        return false;
      if (it->path().filename().string().starts_with("GFZE01") &&
          it->path().extension() == ".ini" && it->path() != games / "GFZE01.ini")
        settings.push_back(it->path());
    }
    if (ec)
      return false;
  }
  for (const auto& path : settings)
    if (IsLink(path))
      return false;
  if (!ImportGXSave(filename, String(folder)))
    return false;
  for (const auto& path : settings)
  {
    Common::IniFile ini;
    if (Exists(path) && !ini.Load(String(path)))
      return false;
    auto* core = ini.GetOrCreateSection("Core");
    core->Set("SlotA", static_cast<int>(ExpansionInterface::EXIDeviceType::MemoryCardFolder));
    core->Set("GCIFolderAPath", String(folder));
    core->Set("GCIFolderAPathOverride", std::string{});
    fs::create_directories(path.parent_path(), ec);
    if (ec || !ini.Save(String(path)))
      return false;
  }
  return true;
}
}  // namespace UICommon
