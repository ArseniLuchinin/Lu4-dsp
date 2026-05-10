#include <IVirtualRX.hpp>
#include <VirtualTransmitter.hpp>

#include <logger.hpp>

static src::severity_channel_logger<logging::trivial::severity_level>
    logger(boost::log::keywords::channel = "IVirtualRX");

IVirtualRX::IVirtualRX() {}

IVirtualRX::~IVirtualRX() = default;

bool IVirtualRX::setTag(const std::string &tag) {
  if (tag.empty()) {
    ERROR << "IVirtualRX::setTag failed: tag is empty." << std::endl;
    return false;
  }

  m_tag = tag;

  // Автоматически регистрируем RX для этого тега (инкремент счётчика)
  VirtualTransmitter::registerRx(tag);

  DEBUG << "IVirtualRX::setTag tag='" << tag << "'." << std::endl;
  return true;
}

std::shared_ptr<IData> IVirtualRX::rxData() {
  if (m_tag.empty()) {
    ERROR << "IVirtualRX::rxData failed: tag is empty." << std::endl;
    return nullptr;
  }

  VirtualTransmitter transmitter;
  auto data = transmitter.waitRxData(m_tag);
  if (data) {
    DEBUG << "IVirtualRX::rxData received data for tag='" << m_tag << "'."
          << std::endl;
  }
  return data;
}
