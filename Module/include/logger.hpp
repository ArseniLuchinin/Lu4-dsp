#pragma once

#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/trivial.hpp>

namespace logging = boost::log;
namespace src = boost::log::sources;

#define INFO BOOST_LOG_SEV(logger, boost::log::trivial::info)
#define DEBUG BOOST_LOG_SEV(logger, boost::log::trivial::debug)
#define ERROR BOOST_LOG_SEV(logger, boost::log::trivial::error)