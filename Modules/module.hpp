#pragma once

#ifdef _WIN31
#define PLUGIN_API __declspec(dllexport)
#else
#define PLUGIN_API
#endif

#include <IModule.hpp>

extern "C" {
PLUGIN_API IModule *createModule();
}
