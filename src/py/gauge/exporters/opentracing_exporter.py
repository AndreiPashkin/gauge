"""Provides :py:class:`OpenTracingExporter` class."""
import contextvars
import logging
import sys
from threading import Lock
from typing import Dict, List, Optional, Tuple

import opentracing

from .. import Span
from ..utils.ttldict import TTLDict

LOGGER = logging.getLogger("gauge")


class OpenTracingExporter:
    """OpenTracing compatible exporter.

    Continuously exports spans using an OpenTracing compatible
    :py:class:`opentracing.Tracer`. Provides an interface compatible with
    aggregators emitting spans.
    """

    def __init__(
        self, tracer: opentracing.Tracer = None, ignore_active_span=True
    ):
        if tracer is None:
            tracer = opentracing.global_tracer()
        self.__tracer = tracer
        self.__ignore_active_span = ignore_active_span

        self.__span_by_id: Dict[int, Tuple[Span, opentracing.Span, bool]] = {}
        # Execution contexts mapped to process ids and thread ids of the
        # spans.
        # This is needed to make it possible to run calls to 'tracer' in
        # separate contexts distinguished by process ids and thread ids.
        # So that tracer implementations that store spans using contextvars
        # (namely Elastic APM Python client) would not mix up spans from
        # different threads.
        self.__context_by_pid_tid = TTLDict(ttl=30.0)

        self.__strip_levels: Dict[
            Tuple[Optional[int], Optional[int]], int
        ] = {}
        self.__lock = Lock()

    def strip_levels(self, count: int, process_id=None, thread_id=None):
        """Make exporter strip 'count' of topmost levels from the output.

        When enabled - stripped levels won't be exported at all and
        the the topmost non-stripped level would become appearing as the
        topmost.
        Main use case is getting rid from infinite spans being exported
        to monitors so there won't be many hours long spans polluting the
        UI.

        Parameters
        ----------
        count
            Count of topmost levels that should be stripped.
        process_id
            PID of spans that should be striped. ``None`` value serves as
            a wildcard.
        thread_id
            Thread ID of spans that should be striped. ``None`` value serves as
            a wildcard.
        """
        with self.__lock:
            key = (process_id, thread_id)
            if count == 0:
                try:
                    del self.__strip_levels[key]
                except KeyError:
                    pass
                return

            self.__strip_levels[(process_id, thread_id)] = count

    def __get_ancestor_spans(self, span: Span):
        while True:
            if span.is_top:
                break
            try:
                parent_span, ot_span, is_stripped = self.__span_by_id[
                    span.parent_id
                ]
            except KeyError:
                break
            yield parent_span, ot_span, is_stripped
            span = parent_span

    def __should_strip(self, span):
        n_ancestors = len(list(self.__get_ancestor_spans(span)))

        for key in [
            (span.process_id, span.thread_id),
            (span.process_id, None),
            (None, span.thread_id),
            (None, None),
        ]:
            try:
                strip_levels = self.__strip_levels[key]
            except KeyError:
                continue
            if (n_ancestors + 1) <= strip_levels:
                return True
        return False

    def __is_top(self, span):
        for _, __, is_stripped in self.__get_ancestor_spans(span):
            if not is_stripped:
                return False

        return True

    def __start_span(self, span):
        if span.id in self.__span_by_id:
            LOGGER.debug(
                "Duplicate span with lifetime 'Start' " "with id '%s'", span.id
            )
            return
        kwargs = {
            "operation_name": span.symbolic_name,
            "start_time": span.timestamp.timestamp(),
            "ignore_active_span": (
                self.__ignore_active_span and self.__is_top(span)
            ),
        }
        if not span.is_top:
            try:
                parent_span, parent_ot_span, is_stripped = self.__span_by_id[
                    span.parent_id
                ]
            except KeyError:
                LOGGER.warning(
                    "Parent span with id '%s' is not found.", span.parent_id
                )
                return
            kwargs["child_of"] = parent_ot_span
            LOGGER.debug(
                "%s is child of %s",
                span.symbolic_name,
                parent_span.symbolic_name,
            )
        else:
            LOGGER.debug("Top span: %s", span.symbolic_name)
        kwargs["tags"] = {
            "hostname": span.hostname,
            "process_id": span.process_id,
            "thread_id": span.thread_id,
            "file_name": span.file_name,
            "line_number": span.line_number,
            "is_coroutine": span.is_coroutine,
            "is_generator": span.is_generator,
        }
        context = self.__context_by_pid_tid.setdefault(
            (span.process_id, span.thread_id), contextvars.Context()
        )
        should_strip = self.__should_strip(span)
        ot_span = None
        if not should_strip:
            ot_span = context.run(self.__tracer.start_span, **kwargs)

        self.__span_by_id[span.id] = (span, ot_span, should_strip)
        LOGGER.debug(
            "Started span with id '%s' and name '%s'%s.",
            span.id,
            span.symbolic_name,
            " as stripped" if should_strip else "",
        )

    def __get_span_by_id(self, span_id, remove=False, quiet=False):
        try:
            if remove:
                return self.__span_by_id.pop(span_id)
            else:
                return self.__span_by_id[span_id]
        except KeyError:
            if not quiet:
                LOGGER.warning("Span with id '%s' is not found.", span_id)
            raise

    def __end_span(self, span):
        try:
            start_span, start_ot_span, is_stripped = self.__get_span_by_id(
                span.id, remove=True
            )
        except KeyError:
            return
        if is_stripped:
            LOGGER.debug(
                "Span '%s' is stripped, skipping ending it.",
                start_span.symbolic_name,
            )
            return

        LOGGER.debug(
            "Ending span '%s'. Duration: %s...",
            start_span.symbolic_name,
            span.timestamp - start_span.timestamp,
        )
        context = self.__context_by_pid_tid.setdefault(
            (span.process_id, span.thread_id), contextvars.copy_context()
        )
        try:
            context.run(
                start_ot_span.finish, finish_time=span.timestamp.timestamp()
            )
        except Exception as exc:
            LOGGER.warning(exc, exc_info=sys.exc_info())

    def __call__(self, spans: List[Span]):
        for span in spans:
            if span.lifetime == Span.SpanLifeTime.Start:
                LOGGER.debug(
                    "Processing start-span with id '%s', name '%s' and "
                    "timestamp '%s'.",
                    span.id,
                    span.symbolic_name,
                    span.timestamp,
                )
                with self.__lock:
                    self.__start_span(span)
            elif span.lifetime == Span.SpanLifeTime.End:
                LOGGER.debug(
                    "Processing end-span with id '%s', name '%s' and "
                    "timestamp '%s'.",
                    span.id,
                    span.symbolic_name,
                    span.timestamp,
                )
                with self.__lock:
                    self.__end_span(span)
