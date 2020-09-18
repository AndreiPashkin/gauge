//
// Created by andrei on 10.05.20.
//

#ifndef GAUGE_COMMON_HPP
#define GAUGE_COMMON_HPP
#include <string>

#include <Python.h>

#include <gauge/utils/benchmark.hpp>
#include <gauge/utils/gil.hpp>

using namespace gauge;

namespace gauge {
namespace detail {

inline std::string safe_encode(PyObject *unicode) {
    detail::GILGuard gil_guard;
    auto bytes  = PyUnicode_AsEncodedString(unicode, "utf-8", "replace");
    auto result = std::string(PyBytes_AsString(bytes));
    Py_DecRef(bytes);
    return result;
}

} // namespace detail
} // namespace gauge

#endif // GAUGE_COMMON_HPP
