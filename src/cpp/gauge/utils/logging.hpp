#ifndef GAUGE_LOGGING_HPP
#define GAUGE_LOGGING_HPP
#include <algorithm>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <gauge/base.hpp>

namespace gauge {
namespace detail {
/**
 * Convert logging level from commonly used one to internal.
 */
spdlog::level::level_enum convert_level(const std::string &level);

/**
 * Create and return global logger. Can only be called once.
 *
 * If all options are turned off - a no-op logger is created.
 *
 * @param stdout Should logger log to stdout.
 * @param stderr Should logger log to stderr.
 * @param level Logging level.
 * @return New logger.
 */
std::shared_ptr<spdlog::logger>
create_logger(bool stdout, bool stderr, spdlog::level::level_enum level);

/**
 * Get or create the project-wide logger
 *
 * If the logger wasn't setup by a call to ::setup_logger(),
 * no-op logger would be used.
 *
 * @return Logger object setup and ready to use.
 */
std::shared_ptr<spdlog::logger> get_logger();
} // namespace detail

/**
 * Setup project-wise logging.
 *
 * Supposed to be called only once during application initialization.
 * If not called a no-op logger would be used and there would be no
 * logging output.
 *
 * @param stdout Whether to write to stdout.
 * @param stderr Whether to write to stderr.
 * @param level Logging level.
 * @return Created logger.
 */
void setup_logging(bool stdout, bool stderr, std::string level);
} // namespace gauge
#endif // GAUGE_LOGGING_HPP
