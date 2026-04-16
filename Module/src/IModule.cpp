#include <IModule.hpp>
#include <Variables.hpp>

void IModule::fetchParam(const std::string& paramName, const std::any& value) {
    const auto token = getVariableToken(value);
    if(not token.empty()){
        const auto value = getValueFromVariable(token);
        setParam(paramName, value);
        return;
    }
    
    setParam(paramName, value);

}
