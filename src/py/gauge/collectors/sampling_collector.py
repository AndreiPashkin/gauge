import datetime as dt

from .base import CollectorInterface
from _gauge import SamplingCollector as SamplingCollectorImpl


class SamplingCollector(CollectorInterface):
    def __init__(
        self,
        sampling_interval: dt.timedelta = dt.timedelta(microseconds=1000),
        processing_interval: dt.timedelta = dt.timedelta(microseconds=1000000)
    ):
        self.__impl = SamplingCollectorImpl(
            sampling_interval=sampling_interval,
            processing_interval=processing_interval
        )

    def subscribe(self, callback: CollectorInterface.CollectCallback):
        self.__impl.subscribe(callback)

    def install(self):
        self.__impl.install()

    def uninstall(self):
        self.__impl.uninstall()

    def start(self):
        return self.__impl.start()

    def pause(self):
        return self.__impl.pause()

    def is_paused(self):
        return self.__impl.is_paused()

    def resume(self):
        return self.__impl.resume()

    def stop(self):
        return self.__impl.stop()

    def is_stopped(self):
        return self.__impl.is_stopped()

    def get_sampling_interval(self):
        return self.__impl.get_sampling_interval()

    def set_sampling_interval(self, interval: dt.timedelta):
        self.__impl.set_sampling_interval(interval)

    def get_collecting_interval(self):
        self.__impl.get_collecting_interval()

    def set_collecting_interval(self, interval: dt.timedelta):
        self.__impl.set_collecting_interval(interval)
