#ifndef MODULE_META_DATA_H
#define MODULE_META_DATA_H

#include <fstream>
#include <string>
#include <typeindex>
#include <unordered_map>

struct ModuleMetaData {
    std::string moduleName;
    std::string libraryFilePath;
    std::string jsonModuleFilePath;
};

class ModuleMethaDataReader {
public:
    explicit ModuleMethaDataReader(const ModuleMetaData& metaData);
    ~ModuleMethaDataReader();

    std::type_index getParamType(const std::string& paramName) const;

private:
    std::ifstream m_fileStream;
    std::unordered_map<std::string, std::type_index> m_paramTypes;

    void loadParams();
    static std::type_index stringToTypeIndex(const std::string& typeStr);
};

#endif
