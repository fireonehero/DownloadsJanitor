#include "FileMover.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <system_error>

namespace {
constexpr std::size_t kMaxCollisionAttempts = 50;
}

FileMover::FileMover(std::filesystem::path watchFolder, std::vector<Rule> rules)
    : m_watchFolder(std::move(watchFolder)), m_rules(std::move(rules)) {
    rebuildLookup();
}

void FileMover::updateRules(std::vector<Rule> rules) {
    m_rules = std::move(rules);
    rebuildLookup();
}

void FileMover::setWatchFolder(std::filesystem::path watchFolder) {
    m_watchFolder = std::move(watchFolder);
}

bool FileMover::organizeOnce() {
    if (m_watchFolder.empty()) {
        std::cerr << "Cannot organize files: watch folder has not been set." << std::endl;
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(m_watchFolder, ec) || ec) {
        std::cerr << "Watch folder `" << m_watchFolder.string() << "` is not accessible: " << (ec ? ec.message() : "path does not exist") << std::endl;
        return false;
    }

    if (!std::filesystem::is_directory(m_watchFolder, ec) || ec) {
        std::cerr << "Watch folder `" << m_watchFolder.string() << "` is not a directory." << std::endl;
        return false;
    }

    bool allSucceeded = true;
    std::filesystem::directory_iterator iter(m_watchFolder, ec);
    if (ec) {
        std::cerr << "Unable to enumerate `" << m_watchFolder.string() << "`: " << ec.message() << std::endl;
        return false;
    }

    for (const auto& entry : iter) {
        ec.clear();
        if (!entry.is_regular_file(ec) || ec) {
            continue;
        }

        const auto& filePath = entry.path();
        const auto destinationDir = resolveDestinationFor(filePath);
        if (destinationDir.empty()) {
            std::cout << "No matching rule for `" << filePath.filename().string() << "`, leaving in place." << std::endl;
            continue;
        }

        std::error_code mkdirErr;
        std::filesystem::create_directories(destinationDir, mkdirErr);
        if (mkdirErr) {
            std::cerr << "Failed to create destination directory `" << destinationDir.string() << "`: " << mkdirErr.message() << std::endl;
            allSucceeded = false;
            continue;
        }

        if (!moveFile(filePath, destinationDir)) {
            allSucceeded = false;
        }
    }

    return allSucceeded;
}

void FileMover::rebuildLookup() {
    m_extensionToDestination.clear();
    for (const auto& rule : m_rules) {
        std::filesystem::path destination(rule.destination);
        if (destination.empty()) {
            continue;
        }

        for (const auto& ext : rule.extensions) {
            std::string normalized = normalizeExtension(ext);
            if (normalized.empty()) {
                continue;
            }

            m_extensionToDestination.emplace(std::move(normalized), destination);
        }
    }
}

std::filesystem::path FileMover::resolveDestinationFor(const std::filesystem::path& file) const {
    std::string extension = file.has_extension() ? normalizeExtension(file.extension().string()) : std::string{};
    if (extension.empty()) {
        return {};
    }

    auto it = m_extensionToDestination.find(extension);
    if (it == m_extensionToDestination.end()) {
        return {};
    }

    return it->second;
}

std::string FileMover::normalizeExtension(std::string extension) {
    extension.erase(std::remove_if(extension.begin(), extension.end(), [](unsigned char ch) {
        return std::isspace(ch);
    }), extension.end());

    if (extension.empty()) {
        return {};
    }

    if (extension.front() != '.') {
        extension.insert(extension.begin(), '.');
    }

    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return extension;
}

bool FileMover::moveFile(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationFolder) const {
    auto targetPath = destinationFolder / sourcePath.filename();

    std::error_code renameErr;
    std::filesystem::rename(sourcePath, targetPath, renameErr);
    if (!renameErr) {
        std::cout << "Moved `" << sourcePath.string() << "` -> `" << targetPath.string() << "`" << std::endl;
        return true;
    }

    std::error_code existsCheckErr;
    bool targetExists = std::filesystem::exists(targetPath, existsCheckErr);
    if (renameErr == std::errc::file_exists || (!existsCheckErr && targetExists)) {
        std::filesystem::path uniquePath;
        std::error_code existsErr;
        for (std::size_t attempt = 1; attempt <= kMaxCollisionAttempts; ++attempt) {
            uniquePath = destinationFolder / (targetPath.stem().string() + "_" + std::to_string(attempt) + targetPath.extension().string());
            bool uniqueExists = std::filesystem::exists(uniquePath, existsErr);
            if (existsErr) {
                std::cerr << "Failed to check for existing file `" << uniquePath.string() << "`: " << existsErr.message() << std::endl;
                existsErr.clear();
                break;
            }

            if (!uniqueExists) {
                std::error_code retryErr;
                std::filesystem::rename(sourcePath, uniquePath, retryErr);
                if (!retryErr) {
                    std::cout << "Moved `" << sourcePath.string() << "` -> `" << uniquePath.string() << "` (renamed to avoid collision)" << std::endl;
                    return true;
                }
                renameErr = retryErr;
            }
        }
    }

    if (renameErr == std::errc::cross_device_link) {
        auto targetPathCopy = destinationFolder / sourcePath.filename();
        std::error_code copyErr;
        std::filesystem::copy_file(sourcePath, targetPathCopy, std::filesystem::copy_options::overwrite_existing, copyErr);
        if (!copyErr) {
            std::error_code removeErr;
            std::filesystem::remove(sourcePath, removeErr);
            if (!removeErr) {
                std::cout << "Copied `" << sourcePath.string() << "` -> `" << targetPathCopy.string() << "` (cross-device move)" << std::endl;
                return true;
            }
            std::cerr << "Failed to remove original file `" << sourcePath.string() << "` after copy: " << removeErr.message() << std::endl;
            return false;
        }

        std::cerr << "Failed to copy `" << sourcePath.string() << "` to `" << targetPathCopy.string() << "`: " << copyErr.message() << std::endl;
        return false;
    }

    std::cerr << "Failed to move `" << sourcePath.string() << "`: " << renameErr.message() << std::endl;
    return false;
}
