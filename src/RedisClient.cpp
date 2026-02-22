#include <RedisClient.hpp>

#include <boost/asio/use_future.hpp>
#include <boost/redis/src.hpp>

#include <cstdlib>
#include <iostream>
#include <tuple>


RedisClient::RedisClient(const std::string& host, const std::string& port)
    : m_context()
    , m_connection(m_context) {
    redis::config cfg;
    cfg.addr.host = host;
    cfg.addr.port = port;
    if (const char* password = std::getenv("REDISCLI_AUTH")) {
        cfg.password = password;
    }

    m_connection.async_run(
        cfg,
        boost::redis::logger{boost::redis::logger::level::emerg},
        [](boost::system::error_code ec) {
            if (ec) {
                std::cerr << "Connection error: " << ec.message() << "\n";
            }
        });
    

    
    std::cout << "Successfully connected to redis server\n";
    m_context.run();
    m_context.restart();
}

std::optional<std::string> RedisClient::get(const std::string& key) {
    redis::request req;
    req.push("GET", key);

    redis::response<std::optional<std::string>> resp;
    auto fut = m_connection.async_exec(req, resp, asio::use_future);

    m_context.run();
    fut.get();
    m_context.restart();

    auto& result = std::get<0>(resp);
    if (!result) {
        return std::nullopt;
    }

    return result.value();
}

void RedisClient::set(const std::string& key, const std::string& value) {
    redis::request req;
    req.push("SET", key, value);

    redis::response<std::string> resp;
    auto fut = m_connection.async_exec(req, resp, asio::use_future);

    m_context.run();
    fut.get();
    m_context.restart();
}
