#include <IModule.hpp>
#include <iostream>
#include <Variables.hpp>
#include <VirtualTransmitter.hpp>

void IModule::fetchParam(const std::string& paramName, const std::any& value) {
    const auto variableToken = getVariableToken(value);
    if(not variableToken.empty()){
        const auto variableValue = getValueFromVariable(variableToken);
        setParam(paramName, variableValue);
        return;
    }

    const auto tagToken = getTagToken(value);
    if (!tagToken.empty()) {
        if (tagToken.size() == 1) {
            setParam(paramName, std::any{});
            return;
        }

        VirtualTransmitter transmitter;
        const std::string tag = tagToken.substr(1);
        transmitter.registerRx(tag);
        auto rxData = transmitter.waitRxData(tag);
        std::cout << "mew" << std::endl;
        setParam(paramName, std::any(rxData));
        return;
    }
    
    setParam(paramName, value);
}

