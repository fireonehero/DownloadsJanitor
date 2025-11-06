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

// Convert a Win32 error code into a trimmed UTF-8 description for logging.
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

// Return the absolute path for the currently running executable, or empty on failure.
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

// Register the janitor to run at logon by writing to the user's Run registry key.
bool registerForStartup(const std::wstring& commandLine) {
    if (commandLine.empty()) {
        std::cerr << "Unable to register startup entry: command line is empty." << std::endl;
        return false;
    }

    HKEY runKey = nullptr;
    LONG status = RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_SET_VALUE, &runKey);
    if (status != ERROR_SUCCESS) {
        std::cerr << "Failed to open startup registry key: " << formatWindowsError(status) << std::endl;
        return false;
    }

    status = RegSetValueExW(runKey,
                            kStartupValueName,
                            0,
                            REG_SZ,
                            reinterpret_cast<const BYTE*>(commandLine.c_str()),
                            static_cast<DWORD>((commandLine.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(runKey);

    if (status != ERROR_SUCCESS) {
        std::cerr << "Failed to register startup entry: " << formatWindowsError(status) << std::endl;
        return false;
    }

    std::wstring_view commandView(commandLine.c_str(), commandLine.size());
    int utf8Length = WideCharToMultiByte(CP_UTF8, 0, commandView.data(), static_cast<int>(commandView.size()),
                                         nullptr, 0, nullptr, nullptr);
    std::string commandUtf8;
    if (utf8Length > 0) {
        commandUtf8.resize(static_cast<std::size_t>(utf8Length));
        WideCharToMultiByte(CP_UTF8, 0, commandView.data(), static_cast<int>(commandView.size()),
                            commandUtf8.data(), utf8Length, nullptr, nullptr);
    } else {
        commandUtf8 = "<unavailable>";
    }

    std::cout << "Startup entry registered successfully: " << commandUtf8 << std::endl;
    return true;
}

// Monitor the watch folder for changes and trigger the mover each time a notification arrives.
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

            // A brief delay keeps duplicate notifications from spinning the loop too quickly.
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
    // Use the executable location so the janitor can ship a bundled config folder.
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

    std::wstring startupCommand;
    if (executablePath.empty()) {
        std::cerr << "Executable path is empty; cannot configure startup." << std::endl;
    } else {
        // Prefer the companion VBScript so the janitor starts hidden, but fall back to the exe.
        std::error_code scriptExistsErr;
        const std::filesystem::path scriptPath =
            executablePath.parent_path().parent_path() / "scripts" / "RunDownloadsJanitorHidden.vbs";

        if (std::filesystem::exists(scriptPath, scriptExistsErr) && !scriptExistsErr) {
            startupCommand = L"wscript.exe \"" + scriptPath.wstring() + L"\"";
            std::cout << "Configuring startup to run via script: " << scriptPath.string() << std::endl;
        } else {
            if (scriptExistsErr) {
                std::cerr << "Unable to validate hidden launcher script: " << scriptExistsErr.message()
                          << ". Falling back to launching the executable directly." << std::endl;
            } else {
                std::cout << "Hidden launcher script not found; falling back to launching the executable directly."
                          << std::endl;
            }
            startupCommand = L"\"" + executablePath.wstring() + L"\"";
        }
    }

    if (!startupCommand.empty()) {
        registerForStartup(startupCommand);
    } else {
        std::cerr << "Startup registration skipped because the command line could not be determined." << std::endl;
    }

    std::cout << "Running DownloadsJanitor once on startup..." << std::endl;
    // Process any new arrivals before entering the long-running watcher loop.
    mover.organizeOnce();

    if (!watchForChanges(watchFolder, mover)) {
        std::cerr << "File monitoring stopped unexpectedly." << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
#endif
}
