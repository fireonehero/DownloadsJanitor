#ifndef FILE_MOVER_HPP
#define FILE_MOVER_HPP

#include "ConfigParser.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

class FileMover {
public:
    FileMover(std::filesystem::path watchFolder, std::vector<Rule> rules);

    bool organizeOnce();
    void updateRules(std::vector<Rule> rules);
    void setWatchFolder(std::filesystem::path watchFolder);

private:
    void rebuildLookup();
    std::filesystem::path resolveDestinationFor(const std::filesystem::path& file) const;
    static std::string normalizeExtension(std::string extension);
    bool moveFile(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationFolder) const;

    std::filesystem::path m_watchFolder;
    std::vector<Rule> m_rules;
    std::unordered_map<std::string, std::filesystem::path> m_extensionToDestination;
};

#endif
