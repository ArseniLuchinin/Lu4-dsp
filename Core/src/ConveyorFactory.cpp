#include <ConveyorFactory.hpp>

#include <boost/system/error_code.hpp>

#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <typeindex>

using boost::json::object;
using boost::json::value;

ConveyorFactory::ConveyorFactory(ModuleFactory &moduleFactory)
    : m_factory(moduleFactory),
      logger(boost::log::keywords::channel = "ConveyorFactory") {}

std::shared_ptr<Conveyor>
ConveyorFactory::createFromJsonString(const std::string &jsonStr) {
  boost::system::error_code ec;
  auto parsed = boost::json::parse(jsonStr, ec);
  if (ec) {
    throw std::runtime_error(std::string("JSON parse error: ") + ec.message());
  }

  const auto *obj = parsed.if_object();
  if (!obj) {
    throw std::runtime_error("Conveyor JSON is not an object");
  }

  return createFromJsonObject(*obj);
}

std::shared_ptr<Conveyor>
ConveyorFactory::createFromJsonObject(const object &conveyorObj) {
  auto nameIt = conveyorObj.find("name");
  if (nameIt == conveyorObj.end() || !nameIt->value().is_string()) {
    throw std::runtime_error("Conveyor 'name' is required");
  }

  const std::string name = nameIt->value().as_string().c_str();
  auto conveyor = std::make_shared<Conveyor>(name);

  auto modulesIt = conveyorObj.find("modules");
  if (modulesIt == conveyorObj.end() || !modulesIt->value().is_array()) {
    throw std::runtime_error("Conveyor 'modules' must be an array");
  }

  const auto &modules = modulesIt->value().as_array();
  for (const auto &modVal : modules) {
    const auto *modObj = modVal.if_object();
    if (!modObj) {
      throw std::runtime_error("Module entry is not an object");
    }

    if (!buildModule(*conveyor, *modObj)) {
      throw std::runtime_error("Failed to build module for conveyor: " + name);
    }
  }

  return conveyor;
}

bool ConveyorFactory::buildModule(Conveyor &conveyor, const object &moduleObj) {
  auto nameIt = moduleObj.find("name");
  if (nameIt == moduleObj.end() || !nameIt->value().is_string()) {
    ERROR << "Module 'name' is required." << std::endl;
    return false;
  }

  const std::string moduleName = nameIt->value().as_string().c_str();
  std::shared_ptr<IModule> module(m_factory.createModule(moduleName));
  if (!module) {
    ERROR << "Failed to create module: " << moduleName << std::endl;
    return false;
  }

  std::string libPath = m_factory.findModule(moduleName);
  ModuleMetaData metaData = module->getMetaData();
  if (!libPath.empty()) {
    metaData.jsonModuleFilePath =
        (std::filesystem::path(libPath).parent_path() /
         metaData.jsonModuleFilePath)
            .string();
  }
  ModuleMethaDataReader reader(metaData);

  if (auto paramsIt = moduleObj.find("params"); paramsIt != moduleObj.end()) {
    if (!paramsIt->value().is_object()) {
      ERROR << "'params' must be an object for module: " << moduleName
            << std::endl;
      return false;
    }

    const auto &params = paramsIt->value().as_object();
    for (const auto &kv : params) {
      const std::string paramName = std::string(kv.key());
      module->fetchParam(paramName,
                         convertParamValue(kv.value(), reader, paramName));
    }
  }

  conveyor.addModule(module);
  return true;
}

std::any ConveyorFactory::jsonToAny(const value &v) {
  if (v.is_string()) {
    return std::string(v.as_string().c_str());
  }
  if (v.is_bool()) {
    return v.as_bool();
  }
  if (v.is_int64()) {
    return static_cast<int64_t>(v.as_int64());
  }
  if (v.is_double()) {
    return v.as_double();
  }
  if (v.is_null()) {
    return std::any{};
  }

  return std::any{};
}

std::any ConveyorFactory::convertParamValue(const value &v,
                                            const ModuleMethaDataReader &reader,
                                            const std::string &paramName) {
  // Строки оставляем как есть — fetchParam разрешит переменные ($VAR) и теги
  // (@tag)
  if (v.is_string()) {
    return std::string(v.as_string().c_str());
  }

  std::type_index expectedType = reader.getParamType(paramName);
  if (expectedType == typeid(void)) {
    return jsonToAny(v);
  }

  if (expectedType == typeid(int64_t)) {
    if (v.is_int64()) {
      return static_cast<int64_t>(v.as_int64());
    }
    if (v.is_double()) {
      return static_cast<int64_t>(v.as_double());
    }
    return jsonToAny(v);
  }

  if (expectedType == typeid(int)) {
    if (v.is_int64()) {
      return static_cast<int>(v.as_int64());
    }
    if (v.is_double()) {
      return static_cast<int>(v.as_double());
    }
    return jsonToAny(v);
  }

  if (expectedType == typeid(double)) {
    if (v.is_double()) {
      return v.as_double();
    }
    if (v.is_int64()) {
      return static_cast<double>(v.as_int64());
    }
    return jsonToAny(v);
  }

  if (expectedType == typeid(bool)) {
    if (v.is_bool()) {
      return v.as_bool();
    }
    return jsonToAny(v);
  }

  if (expectedType == typeid(std::string)) {
    if (v.is_int64()) {
      return std::to_string(v.as_int64());
    }
    if (v.is_double()) {
      return std::to_string(v.as_double());
    }
    if (v.is_bool()) {
      return v.as_bool() ? std::string("true") : std::string("false");
    }
    return jsonToAny(v);
  }

  return jsonToAny(v);
}
