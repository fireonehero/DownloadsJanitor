#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include <vector>
#include <string>

struct Rule {
    std::vector<std::string> extensions;
    std::string destination;
};

class ConfigParser {
    public:
        const std::vector<Rule>&  getRules() const;
        bool load(const std::string& filePath);
        std::string getWatchFolder() const;
    private:
        std::string m_watch_folder;
        std::vector<Rule> m_rules;
};

#endif  
