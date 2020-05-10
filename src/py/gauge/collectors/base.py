from typing import Callable, List

from .. import TraceSample


class CollectorInterface:
    CollectCallback = Callable[[List[TraceSample]], None]

    def subscribe(self, callback: CollectCallback):
        raise NotImplementedError

    def install(self):
        raise NotImplementedError

    def uninstall(self):
        raise NotImplementedError

    def start(self):
        raise NotImplementedError

    def stop(self):
        raise NotImplementedError

    def is_stopped(self):
        raise NotImplementedError
