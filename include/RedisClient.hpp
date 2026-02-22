#ifndef REDIS_CLIENT_HPP
#define REDIS_CLIENT_HPP

#include <boost/asio.hpp>
#include <boost/redis.hpp>
#include <optional>
#include <string>

namespace asio = boost::asio;
namespace redis = boost::redis;

class RedisClient {
public:
    explicit RedisClient(const std::string& host = "127.0.0.1", const std::string& port = "6379");

    std::optional<std::string> get(const std::string& key);
    void set(const std::string& key, const std::string& value);

private:
    asio::io_context m_context;
    redis::connection m_connection;
};

#endif // REDIS_CLIENT_HPP
