#include <deque>
#include <random>
#include <unordered_set>

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <spdlog/spdlog.h>

#include "gauge/span_aggregator.hpp"
#include "gauge/utils/gil.hpp"
#include "gauge/utils/logging.hpp"

using namespace gauge;

SpanAggregator::SpanAggregator(std::chrono::steady_clock::duration span_ttl)
    : span_ttl{span_ttl}, offset{}, logger{detail::get_logger()},
      clocks_base_measurements{
          TimePointConversionUtil::get_base_measurements()},
      random_generator{boost::uuids::random_generator()},
      py_context{py::reinterpret_steal<py::object>(PyContext_New())} {}

void SpanAggregator::execute_callbacks(
    std::shared_ptr<std::vector<std::shared_ptr<Span>>> &spans) {
    if (!spans->empty()) {
        SPDLOG_LOGGER_TRACE(
            logger,
            "Passing {} spans to callbacks...",
            spans->size());
        if (!callbacks.empty()) {
            SPDLOG_LOGGER_TRACE(logger, "Calling callbacks...");
            for (const auto &callback : callbacks) {
                callback(spans);
            }
            SPDLOG_LOGGER_TRACE(logger, "Completed calling callbacks.");
        }
        if (!py_callbacks.empty()) {
            SPDLOG_LOGGER_TRACE(logger, "Calling Python callbacks...");
            for (const auto &callback : py_callbacks) {
                detail::GILGuard gil_guard;
                int              error_code;
                error_code = PyContext_Enter(py_context.ptr());
                if (error_code != 0) {
                    auto msg = "Failed to enter Python contextvars context.";
                    SPDLOG_LOGGER_ERROR(logger, msg);
                    throw std::runtime_error(msg);
                }
                callback(spans);
                error_code = PyContext_Exit(py_context.ptr());
                if (error_code != 0) {
                    auto msg = "Failed to exit Python contextvars context.";
                    SPDLOG_LOGGER_ERROR(logger, msg);
                    throw std::runtime_error(msg);
                }
            }
            SPDLOG_LOGGER_TRACE(logger, "Completed calling Python callbacks.");
        }
    }
}

Span *SpanAggregator::to_end_span(const std::shared_ptr<Span> &span) {
    Span *end_span     = new Span(*span);
    end_span->lifetime = Span::End;
    return end_span;
}

void SpanAggregator::process_open_spans(
    std::shared_ptr<std::vector<std::shared_ptr<Span>>> &spans,
    bool                                                 force_finish) {
    SPDLOG_LOGGER_TRACE(logger, "Detecting complete spans...");
    std::vector<OpenSpan> spans_to_end;
    for (const auto &open_span : open_spans) {
        const auto &span_ptr = open_span.span;
        BOOST_ASSERT_MSG(
            span_ptr->lifetime == Span::Start ||
                span_ptr->lifetime == Span::End,
            "Unexpected Span lifetime.");
        const auto has_span_ttl_expired =
            ((offset - span_ptr->monotonic_clock_timestamp) > span_ttl);
        if (has_span_ttl_expired || force_finish) {
            if (span_ptr->lifetime == Span::Start) {
                auto end_span = std::shared_ptr<Span>(to_end_span(span_ptr));
                add_span(end_span, open_span.cookie, *spans);
                spans_to_end.emplace_back(open_span.cookie, end_span);
            } else if (span_ptr->lifetime == Span::End) {
                spans_to_end.push_back(open_span);
            }
            continue;
        }
    }
    SPDLOG_LOGGER_TRACE(logger, "Ending detected complete spans...");
    auto &by_id_idx = open_spans.get<by_id>();
    for (const auto &open_span : spans_to_end) {
        if (by_id_idx.find(open_span.span->id) != by_id_idx.end()) {
            remove_span(open_span.span, open_span.cookie, *spans);
        }
    }
    SPDLOG_LOGGER_TRACE(logger, "Finished detecting complete spans...");
    SPDLOG_LOGGER_TRACE(logger, "{} pending spans...", open_spans.size());
}

void SpanAggregator::operator()(
    const std::shared_ptr<std::vector<std::shared_ptr<TraceSample>>> &traces) {
    SPDLOG_LOGGER_DEBUG(logger, "Aggregating spans...");
    const std::lock_guard<std::mutex> guard(mutex);

    auto  spans = std::make_shared<std::vector<std::shared_ptr<Span>>>();
    auto &by_cookie_idx = open_spans.get<by_cookie>();

    auto i    = -1;
    auto size = traces->size();
    // Iterate over raw traces and construct new spans from them.
    for (const auto &trace : *traces) {
        i++;
        std::shared_ptr<Span>  parent_span  = nullptr;
        std::shared_ptr<Frame> parent_frame = nullptr;
        if (i == (size - 1)) {
            offset = trace->monotonic_clock_timestamp;
        }
        auto correlation_id = boost::uuids::to_string(random_generator());
        // Iterate over frames starting from the topmost.
        for (auto frames_rev_it = trace->frames->rbegin();
             frames_rev_it != trace->frames->rend();
             frames_rev_it++) {
            auto &frame              = *frames_rev_it;
            auto  span_ptr           = std::make_shared<Span>(*trace, *frame);
            span_ptr->correlation_id = correlation_id;

            auto it = by_cookie_idx.find(frame->cookie);
            if (it == by_cookie_idx.end()) {
                // No open span with such a cookie.
                span_ptr->lifetime = Span::SpanLifeTime::Start;
                span_ptr->id = boost::uuids::to_string(random_generator());
            } else {
                span_ptr->lifetime = Span::SpanLifeTime::End;
                span_ptr->id       = it->span->id;
            }
            if (parent_span != nullptr) {
                span_ptr->parent_id = parent_span->id;
                span_ptr->is_top    = false;
            } else {
                span_ptr->is_top    = true;
                span_ptr->parent_id = {};
            }
            add_span(span_ptr, frame->cookie, *spans);
            parent_span  = span_ptr;
            parent_frame = frame;
        }
    }

    process_open_spans(spans, false);
    sort_spans(spans);
    execute_callbacks(spans);
    SPDLOG_LOGGER_DEBUG(logger, "Completed aggregating spans.");
}

void SpanAggregator::sort_spans(
    const std::shared_ptr<std::vector<std::shared_ptr<Span>>> &spans) {
    std::stable_sort(
        spans->begin(),
        spans->end(),
        [](const std::shared_ptr<Span> &a, const std::shared_ptr<Span> &b) {
            if (a->id == b->parent_id &&
                a->lifetime == Span::SpanLifeTime::Start &&
                b->lifetime == Span::SpanLifeTime::Start) {
                // 'a' is a parent of 'b' and they are both start-spans.
                return true;
            } else if (
                a->parent_id == b->id &&
                a->lifetime == Span::SpanLifeTime::End &&
                b->lifetime == Span::SpanLifeTime::End) {
                // 'a' is a child of 'b' and they are end-spans.
                return true;
            } else if (
                a->lifetime == Span::SpanLifeTime::Start &&
                b->lifetime == Span::SpanLifeTime::End && a->id == b->id) {
                return true;
            } else if (
                a->monotonic_clock_timestamp < b->monotonic_clock_timestamp) {
                return true;
            }
            return false;
        });
#ifndef NDEBUG
    Span *previous = nullptr;
    for (const auto &e : *spans) {
        if (previous != nullptr) {
            if (previous->monotonic_clock_timestamp >
                e->monotonic_clock_timestamp) {
                assert(false);
            }
        }
        previous = e.get();
    }
#endif
    SPDLOG_LOGGER_TRACE(logger, "Completed sorting spans.");
}

void SpanAggregator::finish_open_spans() {
    SPDLOG_LOGGER_DEBUG(logger, "Finishing open spans...");

    const std::lock_guard<std::mutex> guard(mutex);
    auto spans = std::make_shared<std::vector<std::shared_ptr<Span>>>();

    process_open_spans(spans, true);
    sort_spans(spans);
    execute_callbacks(spans);

    SPDLOG_LOGGER_DEBUG(logger, "Finished open spans.");
}

void SpanAggregator::subscribe(
    std::function<void(std::shared_ptr<std::vector<std::shared_ptr<Span>>>)>
        callback) {
    const std::lock_guard<std::mutex> guard(mutex);
    callbacks.emplace_front(std::move(callback));
}

void SpanAggregator::subscribe(py::object callback) {
    const std::lock_guard<std::mutex> guard(mutex);
    py_callbacks.emplace_front(std::move(callback));
}

void SpanAggregator::add_span(
    const std::shared_ptr<Span> &       span,
    const decltype(Frame::cookie) &     cookie,
    std::vector<std::shared_ptr<Span>> &spans) {
    SPDLOG_LOGGER_TRACE(
        logger,
        "Adding open span \"{}\" with id #{}...",
        span->symbolic_name,
        span->id);

    auto &by_id_idx = open_spans.get<by_id>();
    auto  it        = by_id_idx.find(span->id);
    if (it != by_id_idx.end()) {
        // Span is already indexed - replace it.
        BOOST_ASSERT(cookie == it->cookie);
        by_id_idx.replace(it, {cookie, span});
        return;
    };

    auto &by_parent_id_idx = open_spans.get<by_parent_id>();
    auto  sibling_it       = by_parent_id_idx.find(span->parent_id);
    if (sibling_it != by_parent_id_idx.end() &&
        sibling_it->span->process_id == span->process_id &&
        sibling_it->span->thread_id == span->thread_id) {
        // Open sibling exists - end it.
        BOOST_ASSERT(sibling_it->span->id != span->id);
        BOOST_ASSERT(cookie != sibling_it->cookie);
        SPDLOG_LOGGER_TRACE(
            logger,
            "Found opened sibling-span \"{}\" with id #{}. Ending it...",
            sibling_it->span->symbolic_name,
            sibling_it->span->id);
        remove_span(sibling_it->span, sibling_it->cookie, spans);
    }

    open_spans.emplace(cookie, span);

    if (span->lifetime == Span::Start) {
        spans.push_back(span);
        SPDLOG_LOGGER_TRACE(
            logger,
            "Opened span \"{}\" with id #{}...",
            span->symbolic_name,
            span->id);
    }
}

void SpanAggregator::remove_span(
    const std::shared_ptr<Span> &       span,
    const decltype(Frame::cookie) &     cookie,
    std::vector<std::shared_ptr<Span>> &spans) {
    remove_span(
        span,
        cookie,
        spans,
        span->monotonic_clock_timestamp,
        span->timestamp);
}

void SpanAggregator::remove_span(
    const std::shared_ptr<Span> &             span,
    const decltype(Frame::cookie) &           cookie,
    std::vector<std::shared_ptr<Span>> &      spans,
    decltype(Span::monotonic_clock_timestamp) monotonic_clock_timestamp,
    decltype(Span::timestamp)                 timestamp) {
    SPDLOG_LOGGER_TRACE(
        logger,
        "Ending open span \"{}\" with id #{}...",
        span->symbolic_name,
        span->id);
    auto &by_id_idx = open_spans.get<by_id>();
    BOOST_ASSERT(by_id_idx.find(span->id) != by_id_idx.end());

    auto &by_parent_id_idx = open_spans.get<by_parent_id>();
    auto  current_span     = span;
    while (true) {
        auto end_span       = std::shared_ptr<Span>(to_end_span(current_span));
        end_span->timestamp = timestamp;
        end_span->monotonic_clock_timestamp = monotonic_clock_timestamp;
        spans.push_back(end_span);
        by_id_idx.erase(current_span->id);
        SPDLOG_LOGGER_TRACE(
            logger,
            "Ended open span \"{}\" with id #{}...",
            current_span->symbolic_name,
            current_span->id);

        auto child_it = by_parent_id_idx.find(current_span->id);
        if (child_it == by_parent_id_idx.end()) {
            break;
        }
        current_span = child_it->span;
    }
}
