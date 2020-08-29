=====
Gauge
=====
Low-overhead streaming OpenTracing-compatible Python profiler.

Project status
==============
Project in early stage of development and is actively worked on.
It just passed the stage where the main functionality is working correctly
and it is possible to actually install it and use.

Why another profiler?
=====================
The main goal of the project is to build a low-overhead profiler, suitable
for production usage that also would be able to **stream** it's output to
some-kind of monitoring software such as OpenTracing/OpenTelemetry-compatible
monitors.

Features
========
- OpenTracing compatibility.
  Specifically with the following APMs:

  - Elastic APM
    (after https://github.com/elastic/apm-agent-python/pull/824 is merged).
  - Jaeger.
- Very low overhead with 100 / samples per second sampling frequency.

How does it work?
=================
The profiler exploits _PyThread_CurrentFrames() function of CPython's C-API
which provides reasonable performance. This function is called with defined
frequency in a background thread, collects raw data which is in turn
aggregated and exported to somewhere in another background thread which
is activated with much lower frequency.
