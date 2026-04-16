// Helpers to resolve variables in setParam values (no TOML dependency)
#ifndef VARIABLES_RESOLVE_H
#define VARIABLES_RESOLVE_H

#include <any>
#include <string>

std::string getVariableToken(const std::any& value);
std::any getValueFromVariable(const std::string& token);

#endif // VARIABLES_RESOLVE_H
