#ifndef CONVEYOR_H
#define CONVEYOR_H

#include </home/luchinin/my_source/Course_poject/Server/Module/include/IModule.hpp>
#include <logger.hpp>
#include <memory>
#include <string>
#include <vector>

class Conveyor {
public:
  bool init();
  bool run();

  explicit Conveyor(const std::string &name);

  void addModule(std::shared_ptr<IModule> module);
  void removeModule(size_t index);

  const std::vector<std::shared_ptr<IModule>> &getModules() const;
  std::string getName() const;
  bool getIsInitialized() const;

private:
  src::severity_channel_logger<logging::trivial::severity_level> logger;

  std::string m_conveyorName;
  bool m_isInitialized;
  std::vector<std::shared_ptr<IModule>> m_modules;
};

#endif // CONVEYOR_H