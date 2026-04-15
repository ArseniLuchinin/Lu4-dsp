#include <IModule.hpp>

void IModule::fetchParam(const std::string& paramName, const std::any& value) {
    setParam(paramName, resolveParamValue(value));
}
