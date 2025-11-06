#include "ConfigParser.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <system_error>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::string ConfigParser::getWatchFolder() const {
    if (m_watch_folder.empty()) {
        std::cerr << "Watch folder has not been configured yet." << std::endl;
        return {};
    }

    // Confirm the folder exists before handing it back to callers.
    std::error_code ec;
    if (std::filesystem::exists(m_watch_folder, ec)) {
        return m_watch_folder;
    }

    if (ec) {
        std::cerr << "Unable to validate folder `" << m_watch_folder << "`: " << ec.message() << std::endl;
        return {};
    }

    std::cerr << "Invalid folder, please fix it and try again." << std::endl;
    return {};
}

const std::vector<Rule>& ConfigParser::getRules() const {
    return m_rules;
}

bool ConfigParser::load(const std::string& filePath) {
    // rules.json lives in a config folder beside the executable/config root.
    std::filesystem::path configDir = std::filesystem::path(filePath) / "config";
    std::filesystem::path rulesPath = configDir / "rules.json";

    std::ifstream jsonFile(rulesPath);
    if (!jsonFile) {
        std::cerr << "Failed to open configuration file: " << rulesPath << std::endl;
        return false;
    }

    json data;
    try {
        jsonFile >> data;
    } catch (const json::parse_error& e) {
        std::cerr << "Failed to parse configuration file: " << e.what() << std::endl;
        return false;
    }

    loadPlaceholders(data);

    try {
        const std::string rawWatch = data.at("watch_folder").get<std::string>();
        m_watch_folder = applyPlaceholders(rawWatch);
    } catch (const json::exception& e) {
        std::cerr << "Missing or invalid watch_folder: " << e.what() << std::endl;
        return false;
    }

    m_rules.clear();

    bool useDefaultRules = false;
    if (auto it = data.find("use_default_rules"); it != data.end()) {
        if (!it->is_boolean()) {
            std::cerr << "`use_default_rules` must be a boolean value." << std::endl;
            return false;
        }
        useDefaultRules = it->get<bool>();
    }

    const bool hasDefaultRulesSection = data.contains("default_rules");
    const bool hasCustomRulesSection = data.contains("custom_rules");
    const bool hasLegacyRulesSection = data.contains("rules");

    try {
        if (useDefaultRules) {
            if (hasDefaultRulesSection) {
                const std::size_t before = m_rules.size();
                if (!parseRuleArray(data.at("default_rules"), "default_rules")) {
                    return false;
                }
                if (m_rules.size() == before) {
                    std::cout << "`default_rules` is empty; no default rules applied from file." << std::endl;
                }
            } else {
                // Fall back to a curated set so new users get sensible behavior out of the box.
                const auto defaults = builtInDefaultRules(std::filesystem::path(m_watch_folder));
                m_rules.insert(m_rules.end(), defaults.begin(), defaults.end());
                std::cout << "Using built-in default rules (" << defaults.size() << " rule(s))." << std::endl;
            }
        }

        if (hasCustomRulesSection) {
            if (!parseRuleArray(data.at("custom_rules"), "custom_rules")) {
                return false;
            }
        } else if (!useDefaultRules && hasLegacyRulesSection) {
            std::cerr << "The configuration uses the legacy `rules` section; please migrate to `custom_rules` when convenient."
                      << std::endl;
            if (!parseRuleArray(data.at("rules"), "rules")) {
                return false;
            }
        } else if (!useDefaultRules && !hasLegacyRulesSection) {
            std::cerr << "No rules configured. Enable `use_default_rules` or add entries to `custom_rules`." << std::endl;
        }
    } catch (const json::exception& e) {
        std::cerr << "Invalid rules configuration: " << e.what() << std::endl;
        return false;
    }

    std::cout << "Loaded " << m_rules.size() << " rule(s) from " << rulesPath << std::endl;
    return true;
}

bool ConfigParser::parseRuleArray(const json& rulesArray, const std::string& sectionName) {
    if (!rulesArray.is_array()) {
        std::cerr << "Invalid configuration: `" << sectionName << "` must be an array." << std::endl;
        return false;
    }

    for (const auto& ruleJson : rulesArray) {
        if (!ruleJson.is_object()) {
            std::cerr << "Invalid rule entry in `" << sectionName << "`: expected an object." << std::endl;
            return false;
        }

        Rule rule;

        auto extensionsIt = ruleJson.find("extensions");
        if (extensionsIt == ruleJson.end()) {
            std::cerr << "Invalid rule in `" << sectionName << "`: missing `extensions` field." << std::endl;
            return false;
        }

        if (!extensionsIt->is_array()) {
            std::cerr << "Invalid rule in `" << sectionName << "`: `extensions` must be an array." << std::endl;
            return false;
        }

        for (const auto& ext : *extensionsIt) {
            if (!ext.is_string()) {
                std::cerr << "Invalid rule in `" << sectionName << "`: each extension must be a string." << std::endl;
                return false;
            }
            rule.extensions.push_back(ext.get<std::string>());
        }

        if (rule.extensions.empty()) {
            std::cerr << "Invalid rule in `" << sectionName << "`: at least one extension is required." << std::endl;
            return false;
        }

        auto destinationIt = ruleJson.find("destination");
        if (destinationIt == ruleJson.end() || !destinationIt->is_string()) {
            std::cerr << "Invalid rule in `" << sectionName << "`: missing or invalid `destination`." << std::endl;
            return false;
        }

        rule.destination = applyPlaceholders(destinationIt->get<std::string>());
        if (rule.destination.empty()) {
            std::cerr << "Invalid rule in `" << sectionName << "`: destination cannot be empty." << std::endl;
            return false;
        }

        m_rules.push_back(std::move(rule));
    }

    return true;
}

std::vector<Rule> ConfigParser::builtInDefaultRules(const std::filesystem::path& watchFolder) const {
    const std::filesystem::path base = watchFolder.empty() ? std::filesystem::path{} : watchFolder;
    // Helper to either use the watch folder as a base or keep relative subfolders.
    auto makeDestination = [&](const std::string& subFolder) -> std::string {
        if (base.empty()) {
            return subFolder;
        }
        return (base / subFolder).string();
    };

    return {
        {{".exe", ".msi"}, makeDestination("Installers")},
        {{".zip", ".rar", ".7z"}, makeDestination("Archives")},
        {{".jpg", ".jpeg", ".png", ".gif", ".webp"}, makeDestination("Images")},
        {{".pdf"}, makeDestination("PDFs")},
        {{".txt", ".md"}, makeDestination("Notes")},
        {{".mp3", ".wav", ".flac"}, makeDestination("Audio")},
        {{".mp4", ".mkv", ".mov"}, makeDestination("Videos")}
    };
}

void ConfigParser::loadPlaceholders(const json& data) {
    m_placeholders.clear();

    // Support both legacy `user` token and the newer `placeholders` map.
    auto addPlaceholder = [this](const std::string& key, const json& value) {
        if (!value.is_string()) {
            std::cerr << "Placeholder `" << key << "` must be a string." << std::endl;
            return;
        }
        m_placeholders[key] = value.get<std::string>();
    };

    if (auto userIt = data.find("user"); userIt != data.end()) {
        addPlaceholder("user", *userIt);
    }

    if (auto placeholdersIt = data.find("placeholders"); placeholdersIt != data.end()) {
        if (!placeholdersIt->is_object()) {
            std::cerr << "`placeholders` must be an object of key/value strings." << std::endl;
        } else {
            for (auto it = placeholdersIt->begin(); it != placeholdersIt->end(); ++it) {
                addPlaceholder(it.key(), it.value());
            }
        }
    }
}

std::string ConfigParser::applyPlaceholders(const std::string& value) const {
    std::string result = value;
    for (const auto& [key, replacement] : m_placeholders) {
        const std::string token = "{{" + key + "}}";
        std::size_t pos = 0;
        // Replace all occurrences instead of just the first to allow repeated tokens.
        while ((pos = result.find(token, pos)) != std::string::npos) {
            result.replace(pos, token.size(), replacement);
            pos += replacement.size();
        }
    }

    if (result.find("{{") != std::string::npos) {
        std::cerr << "Warning: unresolved placeholder detected in value `" << result << "`." << std::endl;
    }

    return result;
}
