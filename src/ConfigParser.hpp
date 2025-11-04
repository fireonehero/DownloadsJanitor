#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json_fwd.hpp>

struct Rule {
    std::vector<std::string> extensions;
    std::string destination;
};

class ConfigParser {
public:
    const std::vector<Rule>& getRules() const;
    bool load(const std::string& filePath);
    std::string getWatchFolder() const;

private:
    void loadPlaceholders(const nlohmann::json& data);
    std::string applyPlaceholders(const std::string& value) const;
    bool parseRuleArray(const nlohmann::json& rulesArray, const std::string& sectionName);
    std::vector<Rule> builtInDefaultRules(const std::filesystem::path& watchFolder) const;

    std::string m_watch_folder;
    std::vector<Rule> m_rules;
    std::unordered_map<std::string, std::string> m_placeholders;
};

#endif
