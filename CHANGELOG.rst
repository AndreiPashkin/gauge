Change log
==========

0.0.2 (2020-09-12)
------------------
- Documentation-generator setup (#1).
- Initial documentation itself (#1).
- Versioning automation (#15).
- Improved dependencies fetching (#19).

0.0.1 (2020-08-31)
------------------
- Initial working version - correct collection, aggregation and export
  of the profile data is achieved.
- Working support for OpenTracing. Tested with the following APMs:

  - Jaeger.
  - Elastic APM (when this [fix][1] is merged).
- Preliminary benchmarks have shown low performance overhead (< 1%) with
  sampling rate 100 samples per second.

[1]: https://github.com/elastic/apm-agent-python/pull/824
