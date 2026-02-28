#ifndef MODULE_META_DATA_H
#define MODULE_META_DATA_H

#include <string>

struct ModuleMetaData {
    std::string moduleName;
    std::string libraryFilePath;
    std::string jsonModuleFilePath;
};

#endif