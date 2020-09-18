#ifndef GAUGE_SPAN_AGGREGATOR_HPP
#define GAUGE_SPAN_AGGREGATOR_HPP
#include <chrono>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <pybind11/pybind11.h>
#include <spdlog/logger.h>

#include "gauge/base.hpp"
#include "gauge/collector.hpp"
#include "gauge/utils/chrono.hpp"

namespace gauge {
namespace span_aggregator_impl {

namespace py = pybind11;
using namespace std::literals::chrono_literals;
using namespace boost::multi_index;

/**
 * Aggregates raw traces and turns them into spans.
 *
 *
 */
class SpanAggregator {
public:
    explicit SpanAggregator(
        std::chrono::steady_clock::duration span_ttl = 100ms);
    void subscribe(
        std::function<void(
            std::shared_ptr<std::vector<std::shared_ptr<Span>>>)> callback);
    void subscribe(py::object callback);
    void finish_open_spans();

    /**
     * Process raw trace-samples.
     *
     * This is the entry-point of the class, that activates it's main
     * functionality - aggregation of traces.
     *
     * @param traces Raw traces that has to be processed and aggregated.
     */
    void
    operator()(const std::shared_ptr<std::vector<std::shared_ptr<TraceSample>>>
                   &traces);

private:
    using TimePointConversionUtil = detail::TimePointConversionUtil<
        std::chrono::steady_clock,
        std::chrono::system_clock>;
    std::mutex                                mutex;
    std::shared_ptr<spdlog::logger>           logger;
    TimePointConversionUtil::BaseMeasurements clocks_base_measurements;
    std::forward_list<std::function<void(
        std::shared_ptr<std::vector<std::shared_ptr<Span>>>)>>
                                  callbacks;
    std::forward_list<py::object> py_callbacks;

    boost::uuids::random_generator random_generator;

    std::chrono::steady_clock::duration   span_ttl;
    std::chrono::steady_clock::time_point offset;
    std::chrono::steady_clock::time_point finish_offsets;

    /* --- Spans indexing --- */
    // TODO: Need to evaluate idea of having spans indexed as stacks.
    struct OpenSpan {
        decltype(Frame::cookie) cookie;
        std::shared_ptr<Span>   span;

        inline OpenSpan(
            decltype(Frame::cookie) cookie,
            std::shared_ptr<Span>   span)
            : cookie{cookie}, span{std::move(span)} {}
    };

    struct by_cookie {};
    struct by_id {};
    struct by_parent_id {};

    struct span_id_extractor {
        using result_type = decltype(Span::id);

        inline result_type operator()(const OpenSpan &open_span) const {
            return open_span.span->id;
        };
    };
    struct span_parent_id_extractor {
        using result_type = decltype(Span::parent_id);

        inline result_type operator()(const OpenSpan &open_span) const {
            return open_span.span->parent_id;
        };
    };

    multi_index_container<
        OpenSpan,
        indexed_by<
            hashed_unique<
                tag<by_cookie>,
                member<
                    OpenSpan,
                    decltype(OpenSpan::cookie),
                    &OpenSpan::cookie>>,
            hashed_unique<tag<by_id>, span_id_extractor>,
            hashed_non_unique<tag<by_parent_id>, span_parent_id_extractor>>>
        open_spans;
    /**
     * Index new open span
     *
     * Does not mutate the span in any way.
     */
    void add_span(
        const std::shared_ptr<Span> &       span,
        const decltype(Frame::cookie) &     cookie,
        std::vector<std::shared_ptr<Span>> &spans);
    /**
     * Remove indexed span.
     */
    void remove_span(
        const std::shared_ptr<Span> &       span,
        const decltype(Frame::cookie) &     cookie,
        std::vector<std::shared_ptr<Span>> &spans);
    /**
     * @overload void remove_span(
     *               const std::shared_ptr<Span>& span,
     *               const decltype(Frame::cookie) &cookie,
     *               std::vector<std::shared_ptr<Span>> &spans
     *           )
     */
    void remove_span(
        const std::shared_ptr<Span> &             span,
        const decltype(Frame::cookie) &           cookie,
        std::vector<std::shared_ptr<Span>> &      spans,
        decltype(Span::monotonic_clock_timestamp) monotonic_clock_timestamp,
        decltype(Span::timestamp)                 timestamp);

    void process_open_spans(
        std::shared_ptr<std::vector<std::shared_ptr<Span>>> &spans,
        bool force_finish = false);
    Span *     to_end_span(const std::shared_ptr<Span> &span);
    py::object py_context;
    void       sort_spans(
              const std::shared_ptr<std::vector<std::shared_ptr<Span>>> &spans);
    void execute_callbacks(
        std::shared_ptr<std::vector<std::shared_ptr<Span>>> &spans);
};
} // namespace span_aggregator_impl

using span_aggregator_impl::SpanAggregator;

} // namespace gauge
#endif // GAUGE_SPAN_AGGREGATOR_HPP
