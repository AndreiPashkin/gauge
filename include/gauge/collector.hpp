#ifndef GAUGE_COLLECTOR_H
#define GAUGE_COLLECTOR_H
#include <algorithm>
#include <list>
#include <memory>
#include <unordered_set>

#include <Python.h>
#include <pybind11/pybind11.h>

#include "gauge/base.hpp"
#include "gauge/utils/chrono.hpp"

namespace gauge {
namespace collector_impl {

namespace py = pybind11;
using namespace gauge;

/**
 * Collectors are responsible for gathering application's execution state.
 *
 * Collectors gather information about what is being executed and feed
 * it to callbacks registered with subscribe() method.
 */
class CollectorInterface {
public:
    using CallbackInterface = std::function<void(
        const std::shared_ptr<std::vector<std::shared_ptr<TraceSample>>> &)>;
    // TODO: Add a way to unsubscribe a callback (?).
    /**
     * Subscribe a callback that would be called with collected raw data.
     *
     * @param callback A callback to be subscribed.
     */
    virtual void subscribe(CallbackInterface &callback)         = 0;
    virtual void subscribe(py::object callback)                 = 0;
    virtual void start()                                        = 0;
    virtual void resume()                                       = 0;
    virtual bool is_paused()                                    = 0;
    virtual void pause()                                        = 0;
    virtual void stop()                                         = 0;
    virtual bool is_stopped() const                             = 0;
    CollectorInterface()                                        = default;
    CollectorInterface(const CollectorInterface &collector)     = default;
    CollectorInterface(CollectorInterface &&collector) noexcept = default;
    CollectorInterface &
    operator=(const CollectorInterface &collector) = default;
    CollectorInterface &operator=(CollectorInterface &&collector) = default;
    virtual ~CollectorInterface()                                 = default;
};

class CollectorHasAlreadyStarted : public GaugeError {
public:
    const char *what() const noexcept override {
        return "Collector has already started.";
    }
};

class CollectorIsStopped : public GaugeError {
public:
    const char *what() const noexcept override {
        return "Collector is stopped and the operation "
               "is impossible to execute.";
    }
};

class CollectorIsNotPaused : public GaugeError {
public:
    const char *what() const noexcept override {
        return "Collector is not paused and the operation "
               "is impossible to execute.";
    }
};

class CollectorIsAlreadyPaused : public GaugeError {
public:
    const char *what() const noexcept override {
        return "Collector is already paused and the operation "
               "is impossible to execute.";
    }
};

class IncompatibleCollectionCallback : public GaugeError {
public:
    const char *what() const noexcept override {
        return "Provided callback is incompatible with the collector.";
    }
};
} // namespace collector_impl

using collector_impl::CollectorHasAlreadyStarted;
using collector_impl::CollectorInterface;
using collector_impl::CollectorIsAlreadyPaused;
using collector_impl::CollectorIsNotPaused;
using collector_impl::CollectorIsStopped;
using collector_impl::IncompatibleCollectionCallback;

} // namespace gauge

#endif // GAUGE_COLLECTOR_H
