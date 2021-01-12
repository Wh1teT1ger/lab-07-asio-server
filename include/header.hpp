// Copyright 2020 Burylov Denis <burylov01@mail.ru>

#ifndef INCLUDE_HEADER_HPP_
#define INCLUDE_HEADER_HPP_
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include <boost/log/keywords/file_name.hpp>
#include <boost/log/keywords/format.hpp>
#include <boost/log/keywords/rotation_size.hpp>
#include <boost/log/keywords/time_based_rotation.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/trivial.hpp>

namespace logging = boost::log;
namespace sinks = logging::sinks;
namespace keywords = logging::keywords;
namespace expressions = logging::expressions;

using namespace boost::asio;
#endif  // INCLUDE_HEADER_HPP_
