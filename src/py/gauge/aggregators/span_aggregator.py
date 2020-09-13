import datetime as dt
from typing import Callable, List

from .. import Span, TraceSample
from _gauge import SpanAggregator as SpanAggregatorImpl


class SpanAggregator:
    def __init__(
        self, span_ttl: dt.timedelta = dt.timedelta(milliseconds=200)
    ):
        self.__impl = SpanAggregatorImpl(span_ttl=span_ttl)

    def subscribe(self, callback: Callable[[List[Span]], None]):
        self.__impl.subscribe(callback)

    def finish_open_spans(self):
        self.__impl.finish_open_spans()

    def __call__(self, traces: List[TraceSample]):
        self.__impl(traces)
