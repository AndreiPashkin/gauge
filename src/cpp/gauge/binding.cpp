#include <chrono>
#include <memory>
#include <vector>

#include <fmt/format.h>
#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <gauge/base.hpp>
#include <gauge/sampling_collector.hpp>
#include <gauge/span_aggregator.hpp>
#include <gauge/utils/logging.hpp>

namespace py = pybind11;
using namespace gauge;

PYBIND11_MAKE_OPAQUE(std::vector<std::shared_ptr<TraceSample>>);
PYBIND11_MAKE_OPAQUE(std::vector<std::shared_ptr<Frame>>);
PYBIND11_MAKE_OPAQUE(std::vector<std::shared_ptr<Span>>);

// TODO: 1. Make __repr__(), __hash__(), __eq__(), __ne__()
//          for Frame and Trace classes.
PYBIND11_MODULE(_gauge, m) {
    py::bind_vector<
        std::vector<std::shared_ptr<TraceSample>>,
        std::shared_ptr<std::vector<std::shared_ptr<TraceSample>>>>(
        m,
        "TraceSamples");
    py::bind_vector<
        std::vector<std::shared_ptr<Frame>>,
        std::shared_ptr<std::vector<std::shared_ptr<Frame>>>>(m, "Frames");
    py::bind_vector<
        std::vector<std::shared_ptr<Span>>,
        std::shared_ptr<std::vector<std::shared_ptr<Span>>>>(m, "Spans");
    // Exceptions.
    auto gauge_error = py::register_exception<GaugeError>(m, "GaugeError");
    py::register_exception<InvalidLoggingLevel>(
        m,
        "InvalidLoggingLevel",
        gauge_error.ptr());
    py::register_exception<LoggingHasAlreadyBeenSetup>(
        m,
        "LoggingHasAlreadyBeenSetup",
        gauge_error.ptr());
    py::register_exception<CollectorError>(
        m,
        "CollectorError",
        gauge_error.ptr());
    py::register_exception<AggregatorError>(
        m,
        "AggregatorError",
        gauge_error.ptr());

    py::register_exception<ExporterError>(
        m,
        "ExporterError",
        gauge_error.ptr());
    // gauge.Frame
    py::class_<Frame, std::shared_ptr<Frame>>(m, "Frame")
        .def(
            py::init<
                std::string,
                std::string,
                int,
                bool,
                bool,
                unsigned long long>(),
            py::arg("symbolic_name"),
            py::arg("file_name"),
            py::arg("line_number"),
            py::arg("is_coroutine"),
            py::arg("is_generator"),
            py::arg("cookie"))
        .def_readwrite("symbolic_name", &Frame::symbolic_name)
        .def_readwrite("file_name", &Frame::file_name)
        .def_readwrite("line_number", &Frame::line_number)
        .def_readwrite("is_coroutine", &Frame::is_coroutine)
        .def_readwrite("is_generator", &Frame::is_generator)
        .def_readwrite("cookie", &Frame::cookie);
    // gauge.CollectionMethod
    py::enum_<CollectionMethod>(m, "CollectionMethod")
        .value("Sampling", CollectionMethod::Sampling);
    // gauge.Trace
    py::class_<TraceSample, std::shared_ptr<TraceSample>>(m, "TraceSample")
        .def(
            py::init<
                std::shared_ptr<std::vector<std::shared_ptr<Frame>>>,
                std::chrono::steady_clock::time_point,
                std::chrono::system_clock::time_point,
                unsigned long long,
                unsigned long long,
                std::string>(),
            py::arg("frames"),
            py::arg("monotonic_clock_timestamp"),
            py::arg("timestamp"),
            py::arg("thread_id"),
            py::arg("process_id"),
            py::arg("hostname"))
        .def_readonly_static(
            "collection_method",
            &TraceSample::collection_method)
        .def_readwrite("frames", &TraceSample::frames)
        .def_readwrite(
            "monotonic_clock_timestamp",
            &TraceSample::monotonic_clock_timestamp)
        .def_readwrite("timestamp", &TraceSample::timestamp)
        .def_readwrite("thread_id", &TraceSample::thread_id)
        .def_readwrite("process_id", &TraceSample::process_id)
        .def_readwrite("hostname", &TraceSample::hostname);
    // gauge.Span
    py::class_<Span, std::shared_ptr<Span>> PySpan(m, "Span");
    PySpan.def(
        py::init<
            Span::SpanLifeTime,
            std::string,
            std::string,
            std::string,
            bool,
            std::string,
            std::string,
            int,
            bool,
            bool,
            std::chrono::steady_clock::time_point,
            std::chrono::system_clock::time_point,
            unsigned long long,
            unsigned long long,
            std::string>(),
        py::arg("lifetime"),
        py::arg("id"),
        py::arg("parent_id"),
        py::arg("correlation_id"),
        py::arg("is_top"),
        py::arg("symbolic_name"),
        py::arg("file_name"),
        py::arg("line_number"),
        py::arg("is_coroutine"),
        py::arg("is_generator"),
        py::arg("monotonic_clock_timestamp"),
        py::arg("timestamp"),
        py::arg("thread_id"),
        py::arg("process_id"),
        py::arg("hostname"));
    PySpan.def_readwrite("lifetime", &Span::lifetime)
        .def_readwrite("id", &Span::id)
        .def_readwrite("parent_id", &Span::parent_id)
        .def_readwrite("correlation_id", &Span::correlation_id)
        .def_readwrite("is_top", &Span::is_top)
        .def_readwrite("symbolic_name", &Span::symbolic_name)
        .def_readwrite("file_name", &Span::file_name)
        .def_readwrite("line_number", &Span::line_number)
        .def_readwrite("is_coroutine", &Span::is_coroutine)
        .def_readwrite("is_generator", &Span::is_generator)
        .def_readwrite(
            "monotonic_clock_timestamp",
            &Span::monotonic_clock_timestamp)
        .def_readwrite("timestamp", &Span::timestamp)
        .def_readwrite("thread_id", &Span::thread_id)
        .def_readwrite("process_id", &Span::process_id)
        .def_readwrite("hostname", &Span::hostname);
    // gauge.SpanLifetime
    py::enum_<Span::SpanLifeTime>(PySpan, "SpanLifeTime")
        .value("Start", Span::SpanLifeTime::Start)
        .value("End", Span::SpanLifeTime::End);
    // gauge.SamplingCollector
    py::class_<SamplingCollector>(m, "SamplingCollector")
        .def(
            py::init<
                std::chrono::steady_clock::duration,
                std::chrono::steady_clock::duration>(),
            py::arg("sampling_interval"),
            py::arg("processing_interval"))
        .def(
            "subscribe",
            (void (SamplingCollector::*)(py::object)) &
                SamplingCollector::subscribe)
        .def("start", &SamplingCollector::start)
        .def("pause", &SamplingCollector::pause)
        .def("is_paused", &SamplingCollector::is_paused)
        .def("resume", &SamplingCollector::resume)
        .def("stop", &SamplingCollector::stop)
        .def("is_stopped", &SamplingCollector::is_stopped)
        .def(
            "get_sampling_interval",
            &SamplingCollector::get_sampling_interval)
        .def(
            "set_sampling_interval",
            &SamplingCollector::set_sampling_interval)
        .def(
            "get_collection_interval",
            &SamplingCollector::get_collection_interval)
        .def(
            "set_collection_interval",
            &SamplingCollector::set_collection_interval);
    // gauge.SpanAggregator
    py::class_<SpanAggregator>(m, "SpanAggregator")
        .def(
            py::init<std::chrono::steady_clock::duration>(),
            py::arg("span_ttl"))
        .def(
            "subscribe",
            (void (SpanAggregator::*)(py::object)) & SpanAggregator::subscribe)
        .def("finish_open_spans", &SpanAggregator::finish_open_spans)
        .def("__call__", &SpanAggregator::operator(), py::is_operator());
    m.def(
        "setup_logging",
        &gauge::setup_logging,
        py::arg("stdout"),
        py::arg("stderr"),
        py::arg("level"));
}
