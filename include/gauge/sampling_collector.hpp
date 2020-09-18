#ifndef GAUGE_SAMPLING_COLLECTOR_HPP
#define GAUGE_SAMPLING_COLLECTOR_HPP
#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <forward_list>
#include <list>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

#include <Python.h>
#include <pybind11/pybind11.h>
#include <spdlog/logger.h>

#include "gauge/base.hpp"
#include "gauge/collector.hpp"
#include "gauge/utils/chrono.hpp"

namespace gauge {
namespace sampling_collector_impl {

namespace py = pybind11;
using namespace std::literals::chrono_literals;
using namespace gauge;

class SamplingCollector : public CollectorInterface {
public:
    explicit SamplingCollector(
        std::chrono::steady_clock::duration sampling_interval   = 10000us,
        std::chrono::steady_clock::duration processing_interval = 3s,
        bool                                ignore_own_threads  = true);

    ~SamplingCollector() override;

    void subscribe(CallbackInterface &callback) override;
    void subscribe(py::object callback) override;
    void start() override;
    void resume() override;
    bool is_paused() override;
    void pause() override;
    void stop() override;
    bool is_stopped() const override;

    std::chrono::steady_clock::duration get_sampling_interval();

    void set_sampling_interval(
        std::chrono::steady_clock::duration interval) noexcept;

    std::chrono::steady_clock::duration get_collection_interval();

    void set_collection_interval(
        std::chrono::steady_clock::duration interval) noexcept;

private:
    using TimePointConversionUtil = detail::TimePointConversionUtil<
        std::chrono::steady_clock,
        std::chrono::system_clock>;

    struct RawFrame {
        PyFrameObject *                       frame         = nullptr;
        bool                                  is_coroutine  = false;
        bool                                  is_generator  = false;
        bool                                  is_bottommost = false;
        bool                                  is_topmost    = false;
        unsigned long long                    cookie        = 0;
        PyObject *                            thread_id     = nullptr;
        std::chrono::steady_clock::time_point monotonic_clock_timestamp = {};

        RawFrame(
            decltype(frame)                     frame,
            decltype(monotonic_clock_timestamp) monotonic_clock_timestamp,
            decltype(thread_id)                 thread_id,
            bool                                is_topmost    = false,
            bool                                is_bottommost = false);
        RawFrame(RawFrame &&raw_frame) noexcept;

        ~RawFrame();
    };

    /*! Callbacks. */
    std::forward_list<CallbackInterface> collect_callbacks;
    /*! Python callbacks. */
    std::forward_list<py::object> py_collect_callbacks;
    /*! Time workers' loops going to sleep each iteration. */
    static constexpr auto sleep_interval = std::chrono::microseconds(1000);
    /*! Time workers' loops gonna sleep each iteration during pause. */
    static constexpr auto pause_sleep_interval = std::chrono::milliseconds(50);
    static constexpr auto frames_buffer_reserve = 100000;
    std::chrono::steady_clock::duration       sampling_interval;
    std::chrono::steady_clock::duration       processing_interval;
    TimePointConversionUtil::BaseMeasurements clocks_base_measurements;
    std::atomic<bool>                         is_stopped_flag;
    std::atomic<bool>                         is_paused_flag;
    std::atomic<bool>                         ignore_own_threads_flag;
    std::thread                               collector_thread;
    std::thread                               processor_thread;
    // TODO: Maybe a ring-buffer should be used here?
    /**
     * Buffer that stores data collected by collector().
     */
    std::list<RawFrame> raw_frames;
    /**
     * Mutex for class-wise guarding of non-thread safe resources.
     */
    std::mutex mutex;
    /**
     * Mutex for class-wise guarding creation/destruction of threads.
     */
    std::mutex thread_management_mutex;
    /**
     * Logger for class-wise usage.
     */
    std::shared_ptr<spdlog::logger> logger;
    py::object                      py_context;

    std::unordered_set<unsigned long long> own_thread_ids;

    void register_own_thread();

    bool check_if_own_thread(unsigned long long thread_id);

    void clear_own_threads();

    /**
     * Perform profile data sampling in an infinite loop.
     */
    void collector();

    /**
     * Perform sampled raw profile data processing in an infinite loop.
     */
    void processor();

    /**
     * Halt data collecting and processing, process remaining data.
     */
    void finalize(
        bool join_collector_thread = true,
        bool join_processor_thread = true);

    /**
     * Collect data from the interpreter.
     */
    static inline bool collect_frames(
        std::chrono::steady_clock::time_point     monotonic_clock_timestamp,
        std::vector<SamplingCollector::RawFrame> &frames);

    /**
     * Factory function for gauge::Frame objects.
     */
    Frame *construct_frame(const RawFrame &raw_frame);

    /**
     * Factory function for gauge::Trace objects.
     */
    TraceSample *construct_trace(const std::vector<RawFrame *> &raw_frames);
};
} // namespace sampling_collector_impl

using sampling_collector_impl::SamplingCollector;

} // namespace gauge

#endif // GAUGE_SAMPLING_COLLECTOR_HPP
