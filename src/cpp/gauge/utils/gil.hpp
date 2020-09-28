#ifndef GAUGE_GIL_HPP
#define GAUGE_GIL_HPP
#include <Python.h>

namespace gauge {
namespace detail {

class GILGuard {
public:
    GILGuard();
    GILGuard(GILGuard &&) = delete;
    GILGuard &operator=(GILGuard &&) = delete;
    GILGuard(const GILGuard &)       = delete;
    GILGuard &operator=(const GILGuard &) = delete;
    ~GILGuard();

private:
    PyGILState_STATE gil_state;
};

} // namespace detail
} // namespace gauge

#endif // GAUGE_GIL_HPP
