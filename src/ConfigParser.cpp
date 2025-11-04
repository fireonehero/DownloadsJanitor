#include "ConfigParser.hpp"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <system_error>

using json = nlohmann::json;

std::string ConfigParser::getWatchFolder() const {
    if (m_watch_folder.empty()) {
        std::cerr << "Watch folder has not been configured yet." << std::endl;
        return {};
    }

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
    
    try {
        m_watch_folder = data.at("watch_folder").get<std::string>();
    } catch (const json::exception& e) {
        std::cerr << "Missing or invalid watch_folder: " << e.what() << std::endl;
        return false;
    }

    m_rules.clear();

    try {
        const auto& rulesArray = data.at("rules");
        if (!rulesArray.is_array()) {
            std::cerr << "Invalid configuration: `rules` must be an array." << std::endl;
            return false;
        }

        for (const auto& ruleJson : rulesArray) {
            if (!ruleJson.is_object()) {
                std::cerr << "Invalid rule entry: expected an object." << std::endl;
                return false;
            }

            Rule rule;

            const auto& extensions = ruleJson.at("extensions");
            if (!extensions.is_array()) {
                std::cerr << "Invalid rule: `extensions` must be an array." << std::endl;
                return false;
            }

            for (const auto& ext : extensions) {
                if (!ext.is_string()) {
                    std::cerr << "Invalid rule: each extension must be a string." << std::endl;
                    return false;
                }
                rule.extensions.push_back(ext.get<std::string>());
            }

            try {
                rule.destination = ruleJson.at("destination").get<std::string>();
            } catch (const json::exception& e) {
                std::cerr << "Invalid rule: missing or invalid destination: " << e.what() << std::endl;
                return false;
            }
            m_rules.push_back(std::move(rule));
        }
    } catch (const json::exception& e) {
        std::cerr << "Invalid rules configuration: " << e.what() << std::endl;
        return false;
    }

    std::cout << "Loaded " << m_rules.size() << " rule(s) from " << rulesPath << std::endl;
    return true;
}
