// Config variables storage and TOML loader
#ifndef VARIABLES_H
#define VARIABLES_H

#include <any>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include <toml++/toml.h>

class Variables {
public:
    static Variables& instance();

    void registerEnum(const std::string& key,
                      const std::unordered_map<std::string, int>& values);
    bool load(const std::string& filename);

    std::any get(const std::string& key) const;

private:
    Variables() = default;

    bool parseTable(const toml::table& tbl);
    bool processValue(const std::string& key,
                      const toml::node& value);
    bool error(const std::string& key,
               const std::string& message,
               const std::string& location = "") const;

    mutable std::shared_mutex mutex_;
    std::unordered_map<
        std::string,
        std::unordered_map<std::string, int>
    > enums_;
    std::unordered_map<std::string, std::any> data_;
};

// If token starts with '$', tries to resolve it from Variables storage.
// Returns empty std::any when token is not a variable name or not found.
std::any resolveVariableToken(const std::string& token);

#endif // VARIABLES_H
