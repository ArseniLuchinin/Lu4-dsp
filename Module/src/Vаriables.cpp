#include "Variables.hpp"

#include <toml++/toml.h>
#include <iostream>

Config& Config::instance() {
    static Config inst;
    return inst;
}

void Config::registerEnum(const std::string& key,
                          const std::unordered_map<std::string, int>& values) {
    std::unique_lock lock(mutex_);
    enums_[key] = values;
}

bool Config::load(const std::string& filename) {
    std::unique_lock lock(mutex_);

    data_.clear();

    try {
        toml::table tbl = toml::parse_file(filename);
        return parseTable(tbl);
    }
    catch (const toml::parse_error& err) {
        std::cerr << "[Config Error] "
                  << err.description()
                  << " at " << err.source().begin << "\n";
        return false;
    }
}

bool Config::parseTable(const toml::table& tbl) {
    for (const auto& [key, value] : tbl) {
        std::string k{key.str()};

        if (data_.find(k) != data_.end()) {
            return error(k, "duplicate key");
        }

        if (!processValue(k, value)) {
            return false;
        }
    }
    return true;
}

bool Config::processValue(const std::string& key,
                          const toml::node& value) {
    // ENUM (string → int)
    if (value.is_string()) {
        auto str = value.as_string()->get();

        auto eit = enums_.find(key);
        if (eit != enums_.end()) {
            const auto& enumMap = eit->second;

            auto ev = enumMap.find(str);
            if (ev == enumMap.end()) {
                return error(key, "invalid enum value: " + str);
            }

            data_[key] = ev->second;
            return true;
        }

        return error(key, "string not allowed (only enum)");
    }

    // INT
    if (value.is_integer()) {
        data_[key] = static_cast<int32_t>(
            value.as_integer()->get()
        );
        return true;
    }

    // FLOAT
    if (value.is_floating_point()) {
        data_[key] = static_cast<float>(
            value.as_floating_point()->get()
        );
        return true;
    }

    // BOOL
    if (value.is_boolean()) {
        data_[key] = value.as_boolean()->get();
        return true;
    }

    return error(key, "unsupported type");
}

std::any Config::get(const std::string& key) const {
    std::shared_lock lock(mutex_);

    auto it = data_.find(key);
    if (it != data_.end()) {
        return it->second;
    }

    return {};
}

bool Config::error(const std::string& key,
                   const std::string& message,
                   const std::string& location) const {
    std::cerr << "[Config Error] key=\"" << key << "\" "
              << message;

    if (!location.empty()) {
        std::cerr << " at " << location;
    }

    std::cerr << "\n";
    return false;
}
