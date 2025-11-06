#ifndef FILE_MOVER_HPP
#define FILE_MOVER_HPP

#include "ConfigParser.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

// Moves files from the watch folder into destination folders based on extension rules.
class FileMover {
public:
    FileMover(std::filesystem::path watchFolder, std::vector<Rule> rules);

    // Scan the watch folder once and move any matching files; returns false if any move fails.
    bool organizeOnce();
    // Replace the rule set and rebuild the extension lookup table.
    void updateRules(std::vector<Rule> rules);
    // Update the folder being watched; does not rebuild the lookup table.
    void setWatchFolder(std::filesystem::path watchFolder);

private:
    // Regenerate the extension-to-destination cache from the current rules.
    void rebuildLookup();
    // Determine where the provided file should be placed; returns empty if no rule matches.
    std::filesystem::path resolveDestinationFor(const std::filesystem::path& file) const;
    // Normalize extensions (trim whitespace, enforce dot prefix, lower-case).
    static std::string normalizeExtension(std::string extension);
    // Perform the actual filesystem move, handling collisions and cross-device copies.
    bool moveFile(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationFolder) const;

    std::filesystem::path m_watchFolder;
    std::vector<Rule> m_rules;
    std::unordered_map<std::string, std::filesystem::path> m_extensionToDestination;
};

#endif
