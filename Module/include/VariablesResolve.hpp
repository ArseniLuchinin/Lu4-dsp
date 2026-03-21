// Helpers to resolve variables in setParam values (no TOML dependency)
#ifndef VARIABLES_RESOLVE_H
#define VARIABLES_RESOLVE_H

#include <any>
#include <string>

// If value is a string like "$name", tries to resolve it from Variables storage.
// Returns the resolved value or the original value if not a variable.
std::any resolveParamValue(const std::any& value);

#endif // VARIABLES_RESOLVE_H
