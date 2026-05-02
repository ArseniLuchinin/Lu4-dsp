#include <ModuleMetaData.hpp>

#include <boost/json.hpp>

#include <sstream>

ModuleMethaDataReader::ModuleMethaDataReader(const ModuleMetaData& metaData) {
    m_fileStream.open(metaData.jsonModuleFilePath);
    if (m_fileStream.is_open()) {
        loadParams();
    }
}

ModuleMethaDataReader::~ModuleMethaDataReader() {
    if (m_fileStream.is_open()) {
        m_fileStream.close();
    }
}

std::type_index ModuleMethaDataReader::getParamType(const std::string& paramName) const {
    auto it = m_paramTypes.find(paramName);
    if (it != m_paramTypes.end()) {
        return it->second;
    }
    return typeid(void);
}

void ModuleMethaDataReader::loadParams() {
    std::stringstream buffer;
    buffer << m_fileStream.rdbuf();
    std::string content = buffer.str();

    boost::system::error_code ec;
    auto parsed = boost::json::parse(content, ec);
    if (ec) {
        return;
    }

    const auto* obj = parsed.if_object();
    if (!obj) {
        return;
    }

    auto modulesIt = obj->find("modules");
    if (modulesIt == obj->end() || !modulesIt->value().is_array()) {
        return;
    }

    const auto& modules = modulesIt->value().as_array();
    if (modules.empty()) {
        return;
    }

    const auto* firstModule = modules[0].if_object();
    if (!firstModule) {
        return;
    }

    auto fieldsIt = firstModule->find("fields");
    if (fieldsIt == firstModule->end() || !fieldsIt->value().is_array()) {
        return;
    }

    const auto& fields = fieldsIt->value().as_array();
    for (const auto& fieldVal : fields) {
        const auto* fieldObj = fieldVal.if_object();
        if (!fieldObj) {
            continue;
        }

        auto nameIt = fieldObj->find("name");
        auto typeIt = fieldObj->find("type");
        if (nameIt == fieldObj->end() || typeIt == fieldObj->end()) {
            continue;
        }

        if (!nameIt->value().is_string() || !typeIt->value().is_string()) {
            continue;
        }

        std::string name = std::string(nameIt->value().as_string());
        std::string type = std::string(typeIt->value().as_string());
        m_paramTypes.emplace(name, stringToTypeIndex(type));
    }
}

std::type_index ModuleMethaDataReader::stringToTypeIndex(const std::string& typeStr) {
    if (typeStr == "string" || typeStr == "enum") {
        return typeid(std::string);
    }
    if (typeStr == "int") {
        return typeid(int64_t);
    }
    if (typeStr == "real") {
        return typeid(double);
    }
    if (typeStr == "bool") {
        return typeid(bool);
    }
    return typeid(void);
}
