=====
Gauge
=====
Low-overhead streaming (or continuous) OpenTracing-compatible Python profiler.

Quick links
===========
+----------------+------------------------------------------------+
| Documentation  | https://gauge.readthedocs.io/en/stable/        |
+----------------+------------------------------------------------+
| Issue tracker  | https://github.com/AndreiPashkin/gauge/issues/ |
+----------------+------------------------------------------------+

Why another profiler?
=====================
The idea is to make a profiler which would be able to continuously *stream*
the profile data to monitoring software (like Jaeger_ or `Elastic APM`_) and
also, have low-performance overhead so that it would be possible to deploy it on
production. So it would be possible to monitor the performance of a microservice or
some other long-running application in real-time over a long period and explore
historical performance data.

Currently, no open-source alternatives exist that are able to do that.

Features
========
- OpenTracing compatibility.
  Specifically with the following APMs:

  - `Elastic APM`_ (after `this fix`__ is merged).
  - Jaeger_.
- Very low overhead with 100 / samples per second sampling frequency.
- Supports generators/coroutines.

__ https://github.com/elastic/apm-agent-python/pull/824

Compatibility
=============
Currently, the project targets only CPython 3.5+. It is intended to be
compatible with all major operating systems, but currently only tested on
Ubuntu Linux.

Project status
==============
Project in alpha stage of development and is actively worked on.
It just passed the state where the main functionality is working more or less
correctly and it is possible to actually install it and use.

How does it work?
=================
The profiler exploits `_PyThread_CurrentFrames()`_ function of CPython's
C-API which provides reasonable performance. This function is called with
defined frequency in a background thread which collects raw data which is in
turn aggregated and exported to somewhere in another background thread which is
activated with a much lower frequency.

Alternatives
============
+---------------------+------------------+------------------+----------------+-----------------+-------------------------+
| Profiler            | Is low-overhead? | Is streaming?    | Is commercial? | Is Open-Source? | Has permissive license? |
+=====================+==================+==================+================+=================+=========================+
| `DataDog profiler`_ | Yes              | Yes              | Yes            | No              |                         |
+---------------------+------------------+------------------+----------------+-----------------+-------------------------+
| `Py-Spy`_           | Yes              | No               | No             | Yes             | Yes                     |
+---------------------+------------------+------------------+----------------+-----------------+-------------------------+
| `Austin`_           | Yes              | No               | No             | Yes             | No                      |
+---------------------+------------------+------------------+----------------+-----------------+-------------------------+


.. _Jaeger: https://www.jaegertracing.io/
.. _Elastic APM: https://www.elastic.co/apm/
.. _\_PyThread_CurrentFrames(): https://github.com/python/cpython/blob/8ecc0c4d390d03de5cd2344aa44b69ed02ffe470/Python/pystate.c#L1155
.. _DataDog profiler: https://docs.datadoghq.com/tracing/profiler/getting_started/?tab=python
.. _Py-Spy: https://github.com/benfred/py-spy
.. _Austin: https://github.com/P403n1x87/austin
