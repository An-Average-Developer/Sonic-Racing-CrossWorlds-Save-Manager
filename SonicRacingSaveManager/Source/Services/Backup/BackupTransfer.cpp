#include "Core/Files.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Core/Zip.h"
#include "Localization/Localization.h"
#include "Services/Backup/BackupService.h"

namespace SonicRacingSaveManager::Services::Backup {

namespace {

bool EndsWithZip(const std::wstring& path) { return path.size() >= 4 && Text::Lower(path.substr(path.size() - 4)) == L".zip"; }

// ZipFile.CreateFromDirectory: every file under `dir`, entries named by their relative path
bool ZipDirectory(const std::wstring& dir, const std::wstring& zipPath) {
    Zip::Writer writer;
    for (const auto& file : Files::ListFiles(dir, L"*", true)) {
        const auto bytes = Files::ReadBytes(file);
        if (!bytes) return false;
        writer.Add(Text::Replace(Files::RelativePath(dir, file), L"\\", L"/"), *bytes, true, Files::LastWriteTime(file).value_or(0));
    }
    return Files::WriteBytes(zipPath, writer.Finish());
}

// ZipFile.ExtractToDirectory (throws when it fails, as the C# did)
void Extract(const std::wstring& zipPath, const std::wstring& dir) {
    std::wstring error;
    if (!Zip::ExtractToDirectory(zipPath, dir, false, &error))
        throw Error(Loc::Fmt("backup_error_extract", Files::FileName(zipPath)) + (error.empty() ? L"" : L"\n" + error));
}

} // namespace

std::wstring BackupService::ExportBackup(const std::wstring& backupName, std::wstring exportPath) const {
    const std::wstring backupPath = Files::Combine(backupDir, backupName);
    if (!Files::DirectoryExists(backupPath)) throw Error(Loc::Fmt("backup_error_not_found", backupName));
    if (Files::DirectoryExists(exportPath)) exportPath = Files::Combine(exportPath, backupName + L".zip");
    if (!EndsWithZip(exportPath)) exportPath += L".zip";
    if (Files::Exists(exportPath)) Files::Delete(exportPath);
    if (!ZipDirectory(backupPath, exportPath)) throw Error(Loc::Fmt("backup_error_write", exportPath));
    return exportPath;
}

std::wstring BackupService::ImportBackup(const std::wstring& importPath) const {
    if (!Files::Exists(importPath) && !Files::DirectoryExists(importPath)) throw Error(Loc::Fmt("backup_error_import_not_found", importPath));
    const std::wstring backupName = L"imported_" + Time::Stamp(Time::Now());
    const std::wstring backupPath = Files::Combine(backupDir, backupName);
    if (Text::Lower(Files::Extension(importPath)) == L".zip") {
        Extract(importPath, backupPath);
    } else if (Files::DirectoryExists(importPath)) {
        if (!Files::CopyDirectory(importPath, backupPath, true)) throw Error(Loc::Fmt("backup_error_copy", importPath));
    } else {
        throw Error(Loc::Get("backup_error_import_type"));
    }
    return backupName;
}

int BackupService::ImportFilesDirectly(const std::wstring& filePath, const std::wstring& targetAccountId) const {
    if (!Files::Exists(filePath) && !Files::DirectoryExists(filePath)) throw Error(Loc::Fmt("backup_error_file_not_found", filePath));
    const std::wstring targetDir = Files::Combine(baseSaveDir, targetAccountId);
    if (!Files::DirectoryExists(targetDir)) throw Error(Loc::Fmt("backup_error_account_dir_not_found", targetDir));

    int importedCount = 0;
    const std::wstring tempDir = Files::Combine(Files::TempDirectory(), L"SonicSaveImport_" + System::NewGuid());
    // finally { delete the temp folder }
    struct Cleanup {
        const std::wstring& dir;
        ~Cleanup() {
            if (Files::DirectoryExists(dir)) Files::DeleteDirectory(dir);
        }
    } cleanup{ tempDir };

    const std::wstring fileExtension = Text::Lower(Files::Extension(filePath));
    if (fileExtension == L".zip") {
        Files::CreateDirectories(tempDir);
        Extract(filePath, tempDir);
        for (const auto& savFile : Files::ListFiles(tempDir, L"*.sav", true)) {
            const std::wstring fileName = Files::FileName(savFile);
            if (!Files::Copy(savFile, Files::Combine(targetDir, fileName), true)) throw Error(Loc::Fmt("backup_error_copy", fileName));
            ++importedCount;
        }
    } else if (fileExtension == L".sav") {
        const std::wstring fileName = Files::FileName(filePath);
        if (!Files::Copy(filePath, Files::Combine(targetDir, fileName), true)) throw Error(Loc::Fmt("backup_error_copy", fileName));
        ++importedCount;
    } else {
        throw Error(Loc::Fmt("backup_error_unsupported_type", fileExtension));
    }
    if (importedCount == 0) throw Error(Loc::Get("backup_error_no_sav"));
    return importedCount;
}

} // namespace SonicRacingSaveManager::Services::Backup
