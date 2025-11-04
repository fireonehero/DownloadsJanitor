#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include "ConfigParser.hpp"
#include "FileMover.hpp"

#ifdef _WIN32
#include <windows.h>

#include <chrono>
#include <string_view>

namespace {
constexpr wchar_t kStartupValueName[] = L"DownloadsJanitor";
constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr DWORD kWatchFilters = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION;

std::string formatWindowsError(DWORD code) {
    LPWSTR messageBuffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&messageBuffer),
        0,
        nullptr);

    if (length == 0 || messageBuffer == nullptr) {
        return "Unknown error (" + std::to_string(code) + ")";
    }

    std::wstring_view wideMessage(messageBuffer, length);
    int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wideMessage.data(), static_cast<int>(wideMessage.size()), nullptr, 0, nullptr, nullptr);
    std::string message(utf8Length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wideMessage.data(), static_cast<int>(wideMessage.size()), message.data(), utf8Length, nullptr, nullptr);
    LocalFree(messageBuffer);

    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
        message.pop_back();
    }

    return message.empty() ? ("Unknown error (" + std::to_string(code) + ")") : message;
}

std::filesystem::path getExecutablePath() {
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) {
        return {};
    }

    if (length >= buffer.size()) {
        buffer.resize(length + 1);
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
    }

    buffer.resize(length);
    return std::filesystem::path(buffer);
}

bool registerForStartup(const std::filesystem::path& executablePath) {
    if (executablePath.empty()) {
        std::cerr << "Unable to register startup entry: executable path is empty." << std::endl;
        return false;
    }

    HKEY runKey = nullptr;
    LONG status = RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_SET_VALUE, &runKey);
    if (status != ERROR_SUCCESS) {
        std::cerr << "Failed to open startup registry key: " << formatWindowsError(status) << std::endl;
        return false;
    }

    const std::wstring exe = executablePath.wstring();
    status = RegSetValueExW(runKey,
                            kStartupValueName,
                            0,
                            REG_SZ,
                            reinterpret_cast<const BYTE*>(exe.c_str()),
                            static_cast<DWORD>((exe.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(runKey);

    if (status != ERROR_SUCCESS) {
        std::cerr << "Failed to register startup entry: " << formatWindowsError(status) << std::endl;
        return false;
    }

    std::cout << "Startup entry registered successfully." << std::endl;
    return true;
}

bool watchForChanges(const std::filesystem::path& watchFolder, FileMover& mover) {
    std::wstring watchFolderWide = watchFolder.wstring();
    HANDLE changeHandle = FindFirstChangeNotificationW(watchFolderWide.c_str(), FALSE, kWatchFilters);
    if (changeHandle == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to start change notification: " << formatWindowsError(GetLastError()) << std::endl;
        return false;
    }

    std::cout << "Monitoring `" << watchFolder.string() << "` for changes..." << std::endl;

    bool keepWatching = true;
    while (keepWatching) {
        DWORD waitStatus = WaitForSingleObject(changeHandle, INFINITE);
        if (waitStatus == WAIT_OBJECT_0) {
            if (!mover.organizeOnce()) {
                std::cerr << "One or more files failed to move during processing." << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(250));

            if (!FindNextChangeNotification(changeHandle)) {
                std::cerr << "Failed to re-arm change notification: " << formatWindowsError(GetLastError()) << std::endl;
                keepWatching = false;
            }
        } else if (waitStatus == WAIT_FAILED) {
            std::cerr << "WaitForSingleObject failed: " << formatWindowsError(GetLastError()) << std::endl;
            keepWatching = false;
        } else {
            keepWatching = false;
        }
    }

    FindCloseChangeNotification(changeHandle);
    return keepWatching;
}
} // namespace
#endif // _WIN32

int main() {
#ifndef _WIN32
    std::cerr << "DownloadsJanitor currently supports Windows only." << std::endl;
    return EXIT_FAILURE;
#else
    const std::filesystem::path executablePath = getExecutablePath();
    const std::filesystem::path configRoot =
        executablePath.empty() ? std::filesystem::current_path() : executablePath.parent_path();

    ConfigParser parser;
    if (!parser.load(configRoot.string())) {
        std::cerr << "Failed to load configuration. Exiting." << std::endl;
        return EXIT_FAILURE;
    }

    const std::string watchFolderStr = parser.getWatchFolder();
    if (watchFolderStr.empty()) {
        std::cerr << "Watch folder is not configured. Exiting." << std::endl;
        return EXIT_FAILURE;
    }

    std::filesystem::path watchFolder(watchFolderStr);
    std::vector<Rule> rules = parser.getRules();
    if (rules.empty()) {
        std::cerr << "No rules loaded; the janitor will not move files until rules are provided." << std::endl;
    }

    FileMover mover(watchFolder, std::move(rules));
    registerForStartup(executablePath);

    std::cout << "Running DownloadsJanitor once on startup..." << std::endl;
    mover.organizeOnce();

    if (!watchForChanges(watchFolder, mover)) {
        std::cerr << "File monitoring stopped unexpectedly." << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
#endif
}
