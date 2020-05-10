#include <mutex>

#include <spdlog/spdlog.h>
#include <spdlog/common.h>
#include <spdlog/logger.h>

#include "gauge/utils/logging.hpp"

using namespace gauge;


namespace gauge {
    namespace detail {
        static std::shared_ptr<spdlog::logger> logger;
        static std::recursive_mutex logger_mutex;
        static const std::unordered_map<
            std::string, spdlog::level::level_enum
        > levels_map = {
            {"CRITICAL", spdlog::level::level_enum::critical},
            {"ERROR", spdlog::level::level_enum::err},
            {"WARNING", spdlog::level::level_enum::warn},
            {"WARN", spdlog::level::level_enum::warn},
            {"INFO", spdlog::level::level_enum::info},
            {"DEBUG", spdlog::level::level_enum::debug},
            {"TRACE", spdlog::level::level_enum::trace}
        };
    }
}

spdlog::level::level_enum detail::convert_level(const std::string& level) {
    auto it = detail::levels_map.find(level);
    if (it != detail::levels_map.end()) {
        return it->second;
    }
    throw gauge::InvalidLoggingLevel();
}

std::shared_ptr<spdlog::logger> detail::create_logger(
    bool stdout,
    bool stderr,
    spdlog::level::level_enum level
) {
    const std::lock_guard<std::recursive_mutex> guard(detail::logger_mutex);
    if (detail::logger == nullptr) {
        detail::logger = std::make_shared<spdlog::logger>("gauge");
        if (stderr) {
            auto stderr_sink = std::make_shared<
                spdlog::sinks::stderr_color_sink_mt
            >();
            detail::logger->sinks().push_back(stderr_sink);
        }
        if (stdout) {
            auto stdout_sink = std::make_shared<
                spdlog::sinks::stdout_color_sink_mt
            >();
            detail::logger->sinks().push_back(stdout_sink);
        }
        for (auto &sink : detail::logger->sinks()) {
            sink->set_level(level);
            sink->set_pattern(
                "[%Y-%m-%d %H:%M:%S.%e] [%P:%t] [%n] [%l] %v"
            );
        }
        detail::logger->set_level(level);
        detail::logger->set_pattern(
            "[%Y-%m-%d %H:%M:%S.%e] [%P:%t] [%n] [%l] %v"
        );
        return detail::logger;
    } else {
        throw gauge::LoggingHasAlreadyBeenSetup();
    }
}

std::shared_ptr<spdlog::logger> detail::get_logger() {
    const std::lock_guard<std::recursive_mutex> guard(detail::logger_mutex);
    if (detail::logger != nullptr) {
        return detail::logger;
    } else {
        // Make and return a no-op logger.
        return detail::create_logger(
            false,
            false,
            spdlog::level::level_enum::off
        );
    }
}

void gauge::setup_logging(
    bool stdout,
    bool stderr,
    std::string level
) {
    std::transform(level.begin(), level.end(), level.begin(), ::toupper);
    auto internal_level = gauge::detail::convert_level(level);
    gauge::detail::create_logger(stdout, stderr, internal_level);
    spdlog::set_pattern(
        "[%Y-%m-%d %H:%M:%S.%e] [%P:%t] [%n] [%l] %v"
    );
}
