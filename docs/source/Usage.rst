=====
Usage
=====

Installation
============
Gauge can be installed as a regular Python package. Currently, it is not
published on PyPI so you have to clone `the repository`__ to install it.

__ project_

Prerequisites
-------------
- CMake_ above 3.14.
- Some sort of C++ 14 compatible compiler.
- CPython 3.5+.
- CPython's headers.

On recent Debian/Ubuntu version you would be able to install everything
necessary with similar commands::

    sudo apt-get update
    sudo apt-get install build-essential cmake python3.8 python3.8-dev

These exact commands work on Ubuntu 18.04 for example.

Installation steps
------------------
1. Clone the project from `the GitHub repo`__.
2. Execute ``python setup.py install``.

.. warning:: The project doesn't store C++ dependency libraries in the
             repository like some others do, instead - it downloads them on
             demand.
             Downloading can take some time so don't be surprised to find
             yourself waiting a little bit during installation.

That's it.

__ project_

Quickstart
==========
Gauge built on three types of entities - collectors, aggregators, exporters.
Collectors - collect raw data about execution, aggregators - aggregate them,
exporters - push them to somewhere (monitor software).

To instantiate the profiler implementations of these entities has to be
combined.

Minimal example
---------------
.. code-block:: python

    import gauge
    from elasticapm import Client
    from elasticapm.contrib.opentracing import Tracer


    client = Client({
        'SERVICE_NAME': 'my-service',
    })
    tracer = Tracer(client_instance=client)

    collector = gauge.SamplingCollector()
    aggregator = gauge.SpanAggregator()
    collector.subscribe(aggregator)
    exporter = gauge.OpenTracingExporter(tracer=tracer)
    aggregator.subscribe(exporter)
    collector.start()

    # ... work work work

    collector.stop()
    aggregator.finish_open_spans()
    client.close()

.. _CMake: https://cmake.org/
.. _project: https://github.com/AndreiPashkin/gauge/
