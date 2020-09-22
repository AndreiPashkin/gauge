===========
Development
===========
This chapter is dedicated to those who dared to write some code for this
project.

Development environment
=======================
Here is how to create a local development environment:

    1. First of all install everything described in :ref:`Usage:Prerequisites`.
    2. In addition to that, a developer needs to install a debug-build of
       CPython. It is better to use it when developing extensions for CPython -
       you'll get meaningful errors messages in case of reference-counting
       errors for example.

       On Ubuntu 18.04 it could be done with the following command::

           sudo apt-get install python3.8-dbg
    3. Install Tox_ with version above 3.
    4. Use Tox to deploy the development virtualenv::

           tox -edev
    5. Activate the development virtualenv: ``. .tox/dev/bin/activate``.

This is it.

.. _Tox: https://tox.readthedocs.io/en/latest/

Architecture overview
=====================
Gauge is built upon three main entities - collectors, aggregators, exporters.

- Collectors - collect raw data which is just snapshots of the current state of
  execution.
- Aggregators - aggregate the raw data collected by the collector.
  The task of an aggregator is to turn a collection of raw snapshots into
  something meaningful:

    - Spans - potentially incomplete calls with start and end represented as
      separate objects).
    - Calls - complete calls with known start and end.
    - Statistics - summary aggregated data (like it's done in PStats module in
      Python's standard library).
- Exporters - the task of the exporters is to simply save data or push it to
  some kind of remote monitoring service.

Commit message format
=====================
- First line - 50 symbols max. in the imperative mood, conforming to punctuation
  rules of the English language.
- Subsequent lines - 72 symbols max.
- Optionally - comma-separated list of referenced issues/PRs on the last line
  of the message body, preceded by an empty line.

Example good commit message
---------------------------
::

    Add foo() method into Bar class.

    foo() does a really important thing and this makes Bar even more
    awesome.

    #42, #36, #11


Contribution guide
==================
Nothing complex for now - just create an issue on the issue tracker or comment
on the existing one and then make a pull request ;)

Tools
=====
Project has a number of auxiliary static analysis and formatting tools for
Python and C++ code, this section describes how to use them. Python tools are
configured to be used through Tox and C++ tools - through CMake.

Formatters / Style checkers
---------------------------

Python
......

Black
~~~~~
Black_ is a code-formatting tool with minimal configuration, it can format
Python code or check correctness of formatting.

.. _Black: https://github.com/psf/black

Usage
*****
To format code::

    tox -e black

To check code formatting::

    tox -e black -- --check

C++
...

Clang-Format
~~~~~~~~~~~~
Clang-Format_ is a formatting tool for C++ and other languages.

.. _Clang-Format: https://clang.llvm.org/docs/ClangFormat.html

Usage
*****
Before running any commands it's needed to generate build system::

    cmake .

Then - to format code::

    make clang-format

To check code formatting::

    make clang-format-check
