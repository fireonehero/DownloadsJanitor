#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json_fwd.hpp>

// Rule ties a list of extensions to the destination directory that should receive them.
struct Rule {
    std::vector<std::string> extensions;
    std::string destination;
};

// Parses the rules.json file and exposes the resolved watch folder and rules.
class ConfigParser {
public:
    // Read-only access to the loaded rule set.
    const std::vector<Rule>& getRules() const;
    // Load configuration from disk; returns false on I/O or validation errors.
    bool load(const std::string& filePath);
    // Returns the validated watch folder path, or empty string when invalid.
    std::string getWatchFolder() const;

private:
    // Collect placeholder tokens (built-in and user-defined) for later substitution.
    void loadPlaceholders(const nlohmann::json& data);
    // Replace placeholder tokens in strings, logging warnings for unresolved entries.
    std::string applyPlaceholders(const std::string& value) const;
    // Parse and validate a JSON array of rule objects.
    bool parseRuleArray(const nlohmann::json& rulesArray, const std::string& sectionName);
    // Generate a built-in set of rules when requested by the configuration.
    std::vector<Rule> builtInDefaultRules(const std::filesystem::path& watchFolder) const;

    std::string m_watch_folder;
    std::vector<Rule> m_rules;
    std::unordered_map<std::string, std::string> m_placeholders;
};

#endif
