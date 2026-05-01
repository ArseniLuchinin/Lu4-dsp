#ifndef MODULE_META_DATA_H
#define MODULE_META_DATA_H

#include <string>

struct ModuleMetaData {
    std::string moduleName;
    std::string libraryFilePath;
    std::string jsonModuleFilePath;

};

class ModuleMethaDataReader {

public:
    std::type_index getParamType(const std::string& paramName) {
        //todo: возвращает имя перпеданного параметра
    }

    //todo: конструктор. Открывает файл, в деструкторе закрывает
    ModuleMethaDataReader(const ModuleMetaData& metaData){

    }
};

#endif