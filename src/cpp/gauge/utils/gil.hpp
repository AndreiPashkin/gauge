#ifndef GAUGE_GIL_HPP
#define GAUGE_GIL_HPP
#include <Python.h>

namespace gauge {
namespace detail {

class GILGuard {
public:
    GILGuard();
    ~GILGuard();

private:
    PyGILState_STATE gil_state;
};

} // namespace detail
} // namespace gauge

#endif // GAUGE_GIL_HPP
