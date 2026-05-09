#ifndef VIRTUAL_TX_HPP
#define VIRTUAL_TX_HPP

#include <IData.hpp>
#include <IModule.hpp>
#include <any>
#include <memory>
#include <string>

/*! @brief Класс модуля перенаправелния принатых данных в другой конвеер
 * @note Данные принимаются как выход одного конвеера,
 * передаются в модуль поддвержиющи виртуальную передачу
 * Связь <Conveyor> -> <Moudle>
 */
class VirtualTX : public IModule {
public:
  VirtualTX();
  ~VirtualTX() override;

  bool init() override;
  bool run() override;

  void setParam(const std::string &paramName, const std::any &value) override;

  bool setData(std::shared_ptr<IData> data) override;
  std::shared_ptr<IData> getData() override;

private:
  std::string m_tag;
  std::shared_ptr<IData> m_data;
};

#endif // VIRTUAL_TX_HPP
