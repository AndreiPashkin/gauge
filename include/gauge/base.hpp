#ifndef GAUGE_BASE_HPP
#define GAUGE_BASE_HPP

#include <chrono>
#include <exception>
#include <string>

#include <spdlog/logger.h>

namespace gauge {
namespace base_impl {

/**
 * Method of collecting information about program' execution.
 */
enum CollectionMethod {
    /**
     * Collector selects only a subset of events that is incomplete but
     * represents the "big picture".
     */
    Sampling
};
// TODO: Should it be structured with parent-child links for explicitness?
/**
 *  Contains information about currently executing unit.
 */
struct Frame {
    std::string symbolic_name;
    std::string file_name;
    int         line_number;
    bool        is_coroutine;
    // Is the executing unit a generator (anything that "yield"s).
    bool               is_generator;
    unsigned long long cookie;
};

template <const CollectionMethod COLLECTION_METHOD> struct Trace {
    static const CollectionMethod collection_method = COLLECTION_METHOD;
    // Bottommost frame is at the beginning (.begin()).
    std::shared_ptr<std::vector<std::shared_ptr<Frame>>> frames     = {};
    std::chrono::steady_clock::time_point monotonic_clock_timestamp = {};
    std::chrono::system_clock::time_point timestamp                 = {};
    unsigned long long                    thread_id                 = 0;
    unsigned long long                    process_id                = 0;
    std::string                           hostname                  = "";

    Trace(
        std::shared_ptr<std::vector<std::shared_ptr<Frame>>> frames,
        std::chrono::steady_clock::time_point monotonic_clock_timestamp,
        std::chrono::system_clock::time_point timestamp,
        unsigned long long                    thread_id,
        unsigned long long                    process_id,
        std::string                           hostname)
        : frames{std::move(frames)},
          monotonic_clock_timestamp{monotonic_clock_timestamp},
          timestamp{timestamp}, thread_id{thread_id},
          process_id{process_id}, hostname{std::move(hostname)} {};
    Trace() = default;
};

template <const CollectionMethod COLLECTION_METHOD>
const CollectionMethod Trace<COLLECTION_METHOD>::collection_method;

struct TraceSample : public Trace<CollectionMethod::Sampling> {
    using Trace<CollectionMethod::Sampling>::Trace;
};

/**
 * Span represents stream-able state of ongoing execution.
 *
 * The main point of span is that is represents execution as separate
 * events that could be generated as execution progresses and these
 * events could be streamed without waiting for completion of the
 * execution.
 */
struct Span {
    // TODO: Should there be two classes StartSpan and EndSpan so that
    //       there won't be redundancy?
    // TODO: Should there be a state that would represent the intermediate
    //       state between start and end?
    enum SpanLifeTime { Start, End };
    SpanLifeTime                          lifetime = SpanLifeTime::Start;
    std::string                           id{};
    std::string                           parent_id = {};
    std::string                           correlation_id{};
    bool                                  is_top = false;
    std::string                           symbolic_name{};
    std::string                           file_name{};
    int                                   line_number  = 0;
    bool                                  is_coroutine = false;
    bool                                  is_generator = false;
    std::chrono::steady_clock::time_point monotonic_clock_timestamp;
    std::chrono::system_clock::time_point timestamp;
    unsigned long long                    thread_id  = 0;
    unsigned long long                    process_id = 0;
    std::string                           hostname{};

    Span(
        SpanLifeTime                          lifetime,
        std::string                           id,
        std::string                           parent_id,
        std::string                           correlation_id,
        bool                                  is_top,
        std::string                           symbolic_name,
        std::string                           file_name,
        int                                   line_number,
        bool                                  is_coroutine,
        bool                                  is_generator,
        std::chrono::steady_clock::time_point monotonic_clock_timestamp,
        std::chrono::system_clock::time_point timestamp,
        unsigned long long                    thread_id,
        unsigned long long                    process_id,
        std::string                           hostname)
        : lifetime{lifetime}, id{std::move(id)}, parent_id{std::move(
                                                     parent_id)},
          correlation_id{std::move(correlation_id)}, is_top{is_top},
          symbolic_name{std::move(symbolic_name)},
          file_name{std::move(file_name)}, line_number{line_number},
          is_coroutine{is_coroutine}, is_generator{is_generator},
          monotonic_clock_timestamp{monotonic_clock_timestamp},
          timestamp{timestamp}, thread_id{thread_id},
          process_id{process_id}, hostname{std::move(hostname)} {}

    template <typename TraceType>
    Span(const TraceType &trace, const Frame &frame)
        : symbolic_name{frame.symbolic_name}, file_name{frame.file_name},
          line_number{frame.line_number}, is_coroutine{frame.is_coroutine},
          is_generator{frame.is_generator},
          monotonic_clock_timestamp{trace.monotonic_clock_timestamp},
          timestamp{trace.timestamp}, thread_id{trace.thread_id},
          process_id{trace.process_id}, hostname{trace.hostname} {}
    Span() = default;
};

/**
 * Complete call.
 */
struct Call {
    std::string                           id             = {};
    bool                                  is_top         = false;
    std::string                           parent_id      = {};
    std::string                           correlation_id = {};
    std::string                           symbolic_name  = {};
    std::string                           file_name      = {};
    int                                   line_number    = {};
    bool                                  is_coroutine   = {};
    bool                                  is_generator   = {};
    std::chrono::steady_clock::time_point start_monotonic_clock_timestamp;
    std::chrono::steady_clock::time_point end_monotonic_clock_timestamp;
    std::chrono::system_clock::time_point start_timestamp;
    std::chrono::system_clock::time_point end_timestamp;
};

/**
 * Complete trace of calls from top-most to bottom-most.
 */
class CallTrace {
    std::string                           id                              = {};
    unsigned long long                    thread_id                       = 0;
    unsigned long long                    process_id                      = 0;
    std::string                           hostname                        = "";
    std::chrono::steady_clock::time_point start_monotonic_clock_timestamp = {};
    std::chrono::steady_clock::time_point end_monotonic_clock_timestamp   = {};
    std::chrono::system_clock::time_point start_timestamp                 = {};
    std::chrono::system_clock::time_point end_timestamp                   = {};
};

/**
 * Base library-wise exception class.
 */
class GaugeError : public std::exception {
public:
    const char *what() const noexcept override {
        return "General Gauge exception.";
    }
};

class InvalidLoggingLevel : public GaugeError {
public:
    const char *what() const noexcept override {
        return "Invalid logging level.";
    }
};

class LoggingHasAlreadyBeenSetup : public GaugeError {
public:
    const char *what() const noexcept override {
        return "Logging has already been setup. It can be done only once.";
    }
};

/**
 * Base collector-related exception class.
 */
class CollectorError : public GaugeError {
public:
    const char *what() const noexcept override {
        return "General Collector exception.";
    }
};

/**
 * Base aggregator-related exception class.
 */
class AggregatorError : public GaugeError {
public:
    const char *what() const noexcept override {
        return "General Aggregator exception.";
    }
};

/**
 * Base exporter-related exception class.
 */
class ExporterError : public GaugeError {
public:
    const char *what() const noexcept override {
        return "General Export exception.";
    }
};
} // namespace base_impl

using base_impl::AggregatorError;
using base_impl::Call;
using base_impl::CallTrace;
using base_impl::CollectionMethod;
using base_impl::CollectorError;
using base_impl::ExporterError;
using base_impl::Frame;
using base_impl::GaugeError;
using base_impl::InvalidLoggingLevel;
using base_impl::LoggingHasAlreadyBeenSetup;
using base_impl::Span;
using base_impl::Trace;
using base_impl::TraceSample;

} // namespace gauge

#endif // GAUGE_BASE_HPP
