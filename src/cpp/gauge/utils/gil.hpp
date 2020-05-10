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

}
}

#endif //GAUGE_GIL_HPP
