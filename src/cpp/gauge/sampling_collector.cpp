#include <functional>
#include <list>
#include <vector>

#include <Python.h>

#include <boost/asio/ip/host_name.hpp>
#include <boost/process/environment.hpp>
#include <spdlog/spdlog.h>

#include "gauge/sampling_collector.hpp"
#include "gauge/utils/benchmark.hpp"
#include "gauge/utils/chrono.hpp"
#include "gauge/utils/common.hpp"
#include "gauge/utils/gil.hpp"
#include "gauge/utils/logging.hpp"

using namespace gauge;

// TODO: Replace unsigned long with duration?
SamplingCollector::SamplingCollector(
    std::chrono::steady_clock::duration sampling_interval,
    std::chrono::steady_clock::duration processing_interval,
    bool                                ignore_own_threads)
    : is_stopped_flag{true}, is_paused_flag{false},
      ignore_own_threads_flag{ignore_own_threads},
      sampling_interval{sampling_interval},
      processing_interval{processing_interval},
      clocks_base_measurements{
          TimePointConversionUtil::get_base_measurements()},
      logger{detail::get_logger()},
      py_context(py::reinterpret_steal<py::object>(PyContext_New())),
      own_thread_ids{} {}

SamplingCollector::~SamplingCollector() { finalize(); }

void SamplingCollector::subscribe(CallbackInterface &callback) {
    const std::lock_guard<std::mutex> guard(mutex);
    collect_callbacks.push_front(callback);
}

void SamplingCollector::subscribe(py::object callback) {
    const std::lock_guard<std::mutex> guard(mutex);
    py_collect_callbacks.emplace_front(std::move(callback));
}

void SamplingCollector::start() {
    SPDLOG_LOGGER_DEBUG(logger, "Starting profiler...");
    const std::lock_guard<std::mutex> guard(thread_management_mutex);
    if (!is_stopped_flag) {
        throw CollectorHasAlreadyStarted();
    }
    is_stopped_flag = false;

    processor_thread = std::thread([this] { this->processor(); });
    collector_thread = std::thread([this] { this->collector(); });
    SPDLOG_LOGGER_DEBUG(logger, "Started sampling collector.");
}
void SamplingCollector::stop() { finalize(); }
bool SamplingCollector::is_stopped() const { return is_stopped_flag; }

void SamplingCollector::register_own_thread() {
    unsigned long long own_thread_id;
    {
        detail::GILGuard gil_guard;
        own_thread_id = py::cast<unsigned long long>(
            py::module::import("threading").attr("get_ident")());
    }
    own_thread_ids.insert(own_thread_id);
}

bool SamplingCollector::check_if_own_thread(unsigned long long thread_id) {
    return own_thread_ids.count(thread_id) != 0;
}

void SamplingCollector::clear_own_threads() { own_thread_ids.clear(); }

void SamplingCollector::finalize(
    bool join_collector_thread,
    bool join_processor_thread) {
    SPDLOG_LOGGER_DEBUG(logger, "Finalizing SamplingProfiler...");
    const std::lock_guard<std::mutex> thread_management_guard(
        thread_management_mutex);
    pybind11::gil_scoped_release release;
    if (is_stopped_flag) {
        SPDLOG_LOGGER_DEBUG(
            logger,
            "SamplingProfiler has already stopped, doing nothing...");
        return;
    }
    is_stopped_flag = true;
    if (collector_thread.joinable() && join_collector_thread) {
        SPDLOG_LOGGER_TRACE(logger, "Joining the collector thread...");
        collector_thread.join();
    }
    if (processor_thread.joinable() && join_processor_thread) {
        SPDLOG_LOGGER_TRACE(logger, "Joining the processor thread...");
        processor_thread.join();
    }
    {
        const std::lock_guard<std::mutex> guard(mutex);
        clear_own_threads();
    }
    SPDLOG_LOGGER_DEBUG(logger, "Finalized SamplingProfiler.");
}

std::chrono::steady_clock::duration
SamplingCollector::get_sampling_interval() {
    const std::lock_guard<std::mutex> guard(mutex);
    return sampling_interval;
}

void SamplingCollector::set_sampling_interval(
    std::chrono::steady_clock::duration interval) noexcept {
    const std::lock_guard<std::mutex> guard(mutex);
    sampling_interval = interval;
}

std::chrono::steady_clock::duration
SamplingCollector::get_collection_interval() {
    const std::lock_guard<std::mutex> guard(mutex);
    return processing_interval;
}

void SamplingCollector::set_collection_interval(
    std::chrono::steady_clock::duration interval) noexcept {
    const std::lock_guard<std::mutex> guard(mutex);
    processing_interval = interval;
}

static Timer timer;

void SamplingCollector::collector() {
    auto previous_timestamp = std::chrono::steady_clock::now();
    bool has_sampling_interval_passed;

    SPDLOG_LOGGER_DEBUG(logger, "Launching profile data sampling...");
    {
        const std::lock_guard<std::mutex> guard(mutex);
        register_own_thread();
    }
    std::vector<RawFrame> frames_buffer;
    // Essentially vector with reserved memory is used as a memory pool.
    frames_buffer.reserve(frames_buffer_reserve);
    const auto half_sleep_interval = sleep_interval / 2;

    auto t1               = std::chrono::steady_clock::now();
    auto profile_interval = std::chrono::seconds(1);
    while (true) {
        if (is_stopped_flag) {
            break;
        }
        if (is_paused_flag) {
            std::this_thread::sleep_for(pause_sleep_interval);
            continue;
        }
        auto current_timestamp = std::chrono::steady_clock::now();
        {
            const std::lock_guard<std::mutex> guard(mutex);
            has_sampling_interval_passed =
                ((current_timestamp - previous_timestamp) >=
                 sampling_interval);
        }
        previous_timestamp = current_timestamp;
        if (has_sampling_interval_passed) {
            std::this_thread::sleep_for(half_sleep_interval);
            //            timer.start("collect_frames");
            auto result = collect_frames(current_timestamp, frames_buffer);
            //            timer.stop("collect_frames");
            //            if ((std::chrono::steady_clock::now() - t1) >=
            //            profile_interval) {
            //                SPDLOG_LOGGER_INFO(
            //                    logger,
            //                    "collect_frames -> {:f}",
            //                    timer.get_average("collect_frames")
            //                );
            //            }
            std::this_thread::sleep_for(half_sleep_interval);
            if (!result) {
                SPDLOG_LOGGER_WARN(
                    logger,
                    "Error during sample gathering. Skipping...");
                continue;
            }

            const std::lock_guard<std::mutex> guard(mutex);
            raw_frames.insert(
                raw_frames.end(),
                std::make_move_iterator(frames_buffer.begin()),
                std::make_move_iterator(frames_buffer.end()));
            frames_buffer.clear();
            if (frames_buffer.capacity() != frames_buffer_reserve) {
                SPDLOG_LOGGER_TRACE(
                    logger,
                    "Capacity of traces buffer has changed... "
                    "Adjusting to default...");
                frames_buffer.shrink_to_fit();
                frames_buffer.reserve(frames_buffer_reserve);
            }
        } else {
            std::this_thread::sleep_for(sleep_interval);
        }
    }
    SPDLOG_LOGGER_DEBUG(logger, "Profile data sampling has stopped.");
}

void SamplingCollector::processor() {
    auto previous_processing_timestamp = std::chrono::steady_clock::now();
    bool has_processing_interval_passed;

    SPDLOG_LOGGER_DEBUG(logger, "Launching processing...");
    try {
        {
            const std::lock_guard<std::mutex> guard(mutex);
            register_own_thread();
        }
        while (true) {
            if (is_stopped_flag && raw_frames.empty()) {
                break;
            }
            if (is_paused_flag && raw_frames.empty()) {
                std::this_thread::sleep_for(pause_sleep_interval);
                continue;
            }
            std::this_thread::sleep_for(sleep_interval);

            if (!is_stopped_flag) {
                auto current_processing_timestamp =
                    (std::chrono::steady_clock::now());
                {
                    const std::lock_guard<std::mutex> guard(mutex);
                    has_processing_interval_passed =
                        ((current_processing_timestamp -
                          previous_processing_timestamp) >=
                         processing_interval);
                }
                if (!has_processing_interval_passed) {
                    continue;
                }
                previous_processing_timestamp = current_processing_timestamp;
            }

            auto begin = raw_frames.begin();
            auto end   = raw_frames.end();

#ifndef NDEBUG
            {
                auto it         = begin;
                auto prev_frame = it;
                it++;
                for (; it != end; it++) {
                    assert(
                        prev_frame->monotonic_clock_timestamp <=
                        it->monotonic_clock_timestamp);
                    prev_frame = it;
                }
            }
#endif
            auto traces =
                std::make_shared<std::vector<std::shared_ptr<TraceSample>>>();

            SPDLOG_LOGGER_TRACE(logger, "Processing raw traces...");
            auto last_topmost_frame = end;
            --last_topmost_frame;
            for (; last_topmost_frame != begin; last_topmost_frame--) {
                if (last_topmost_frame->is_topmost) {
                    end = ++last_topmost_frame;
                    break;
                }
            }
            last_topmost_frame = end;
            last_topmost_frame--;
            BOOST_ASSERT(begin == end || last_topmost_frame->is_topmost);
            for (auto it = begin; it != end;) {
                auto frames = std::vector<RawFrame *>();
                while (true) {
                    frames.emplace_back(&(*it));
                    if (it->is_topmost) {
                        it++;
                        break;
                    }
                    it++;
                }
                // Extract necessary information from the raw data
                // into specialized structures.
                auto trace = construct_trace(frames);
                {
                    const std::lock_guard<std::mutex> guard(mutex);
                    if (check_if_own_thread(trace->thread_id)) {
                        SPDLOG_LOGGER_TRACE(
                            logger,
                            "Skipping trace with thread ID \"{}\" as own...",
                            trace->thread_id);
                        continue;
                    }
                }
                traces->emplace_back(trace);
            }
            {
                detail::GILGuard gil_guard;
                raw_frames.erase(begin, end);
            }
            SPDLOG_LOGGER_TRACE(logger, "Processed raw traces.");
#ifndef NDEBUG
            {
                TraceSample *prev_trace = nullptr;
                for (auto const &item : *traces) {
                    if (prev_trace != nullptr) {
                        assert(
                            prev_trace->monotonic_clock_timestamp <=
                            item->monotonic_clock_timestamp);
                    }
                    prev_trace = item.get();
                }
            }
#endif
            if (!traces->empty()) {
                if (!collect_callbacks.empty()) {
                    SPDLOG_LOGGER_TRACE(logger, "Calling callbacks...");
                    for (auto &callback : collect_callbacks) {
                        callback(traces);
                    }
                    SPDLOG_LOGGER_TRACE(
                        logger,
                        "Completed calling callbacks.");
                }
                if (!py_collect_callbacks.empty()) {
                    SPDLOG_LOGGER_TRACE(logger, "Calling Python callbacks...");
                    for (const auto &callback : py_collect_callbacks) {
                        detail::GILGuard gil_guard;
                        int              error_code;
                        error_code = PyContext_Enter(py_context.ptr());
                        if (error_code != 0) {
                            auto msg = "Failed to enter Python "
                                       "contextvars context.";
                            SPDLOG_LOGGER_ERROR(logger, msg);
                            throw std::runtime_error(msg);
                        }
                        (*callback)(traces);
                        error_code = PyContext_Exit(py_context.ptr());
                        if (error_code != 0) {
                            auto msg = "Failed to exit Python "
                                       "contextvars context.";
                            SPDLOG_LOGGER_ERROR(logger, msg);
                            throw std::runtime_error(msg);
                        }
                    }
                    SPDLOG_LOGGER_TRACE(
                        logger,
                        "Completed calling Python callbacks.");
                }
            }
        }
    } catch (const std::exception &e) {
        SPDLOG_LOGGER_ERROR(
            logger,
            "Processing has stopped due to the exception: \"{}\".",
            e.what());
        finalize(false, false);
        return;
    }
    SPDLOG_LOGGER_DEBUG(logger, "Processing has stopped.");
}

// TODO: It is possible to re-implement it using:
//       https://docs.python.org/3/c-api/init.html#advanced-debugger-support
//       This way also context ID could be retrieved to be used as an
//       additional frame attribute along with process_id and so on. If it
//       could be useful at all.
bool SamplingCollector::collect_frames(
    std::chrono::steady_clock::time_point monotonic_clock_timestamp,
    std::vector<RawFrame> &               frames) {
    detail::GILGuard gil_guard;
    PyObject *       raw_frames = _PyThread_CurrentFrames();
    if (raw_frames == nullptr) {
        return false;
    }

    PyObject * thread_id, *value;
    Py_ssize_t position = 0;
    while (PyDict_Next(raw_frames, &position, &thread_id, &value)) {
        auto frame         = reinterpret_cast<PyFrameObject *>(value);
        bool is_bottommost = true;
        bool is_topmost    = false;
        while (frame != nullptr) {
            is_topmost = frame->f_back == nullptr;
            frames.emplace_back(
                frame,
                monotonic_clock_timestamp,
                thread_id,
                is_topmost,
                is_bottommost);
            is_bottommost = false;
            frame         = frame->f_back;
        }
    }
    return true;
}

SamplingCollector::RawFrame::RawFrame(
    decltype(frame)                     frame,
    decltype(monotonic_clock_timestamp) monotonic_clock_timestamp,
    decltype(thread_id)                 thread_id,
    bool                                is_topmost,
    bool                                is_bottommost)
    : monotonic_clock_timestamp{monotonic_clock_timestamp},
      is_topmost{is_topmost}, is_bottommost{is_bottommost} {
    Py_XINCREF(thread_id);
    this->thread_id = thread_id;

    Py_XINCREF(reinterpret_cast<PyObject *>(frame));
    this->frame = frame;
    // Use address of the frame object as a cookie assuming
    // that for the same function there would be the same frame with the
    // same memory address (which is also the object's ID in CPython).
    cookie = reinterpret_cast<unsigned long long int>(frame);
    if (frame->f_gen == nullptr) {
        is_coroutine = false;
        is_generator = false;
    } else if (PyCoro_CheckExact(frame->f_gen)) {
        is_coroutine = true;
        is_generator = false;
    } else if (PyGen_Check(frame->f_gen)) {
        is_coroutine = false;
        is_generator = true;
    } else {
        SPDLOG_LOGGER_WARN(detail::get_logger(), "Unexpected f_gen value.");
    }
}

SamplingCollector::RawFrame::~RawFrame() {
    Py_XDECREF(thread_id);
    Py_XDECREF(reinterpret_cast<PyObject *>(frame));
}

SamplingCollector::RawFrame::RawFrame(RawFrame &&raw_frame) noexcept
    : frame{raw_frame.frame}, is_coroutine{raw_frame.is_coroutine},
      is_generator{raw_frame.is_generator},
      is_bottommost{raw_frame.is_bottommost}, is_topmost{raw_frame.is_topmost},
      cookie{raw_frame.cookie}, thread_id{raw_frame.thread_id},
      monotonic_clock_timestamp{raw_frame.monotonic_clock_timestamp} {
    raw_frame.frame     = nullptr;
    raw_frame.thread_id = nullptr;
}

Frame *SamplingCollector::construct_frame(const RawFrame &raw_frame) {
    auto frame = new Frame{};

    // TODO: Implement retrieval of fully qualified name of the object.

    frame->symbolic_name =
        detail::safe_encode(raw_frame.frame->f_code->co_name);
    frame->file_name =
        detail::safe_encode(raw_frame.frame->f_code->co_filename);
    frame->line_number  = raw_frame.frame->f_lineno;
    frame->is_coroutine = raw_frame.is_coroutine;
    frame->is_generator = raw_frame.is_generator;
    frame->cookie       = raw_frame.cookie;
    return frame;
}

TraceSample *
SamplingCollector::construct_trace(const std::vector<RawFrame *> &raw_frames) {
    BOOST_ASSERT(!raw_frames.empty());
    auto trace_sample = new TraceSample{};
    trace_sample->frames =
        std::make_shared<std::vector<std::shared_ptr<Frame>>>();
    trace_sample->monotonic_clock_timestamp =
        raw_frames[0]->monotonic_clock_timestamp;
    trace_sample->timestamp = TimePointConversionUtil::convert_time_point(
        raw_frames[0]->monotonic_clock_timestamp,
        clocks_base_measurements);
    trace_sample->process_id = boost::this_process::get_id();
    trace_sample->hostname   = boost::asio::ip::host_name();

    {
        detail::GILGuard gil_guard;
        trace_sample->thread_id =
            py::cast<unsigned long long>(raw_frames[0]->thread_id);
    }

    for (const auto &raw_frame : raw_frames) {
        trace_sample->frames->emplace_back(construct_frame(*raw_frame));
    }
    return trace_sample;
}

void SamplingCollector::resume() {
    if (is_stopped()) {
        throw CollectorIsStopped();
    }
    if (!is_paused_flag) {
        throw CollectorIsNotPaused();
    }
    is_paused_flag = false;
}

bool SamplingCollector::is_paused() { return is_paused_flag; }

void SamplingCollector::pause() {
    if (is_paused_flag) {
        throw CollectorIsAlreadyPaused();
    }
    is_paused_flag = true;
}

constexpr std::chrono::microseconds SamplingCollector::sleep_interval;
constexpr std::chrono::milliseconds SamplingCollector::pause_sleep_interval;
constexpr int                       SamplingCollector::frames_buffer_reserve;
