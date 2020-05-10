#include <gauge/utils/gil.hpp>

using namespace gauge;

detail::GILGuard::GILGuard() :
    gil_state(PyGILState_Ensure())
    {}

detail::GILGuard::~GILGuard() {
    PyGILState_Release(gil_state);
}
