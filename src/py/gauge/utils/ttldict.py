""""""
import time
from threading import Lock
from collections.abc import MutableMapping


class TTLDict(MutableMapping):
    """Dictionary where keys are expiring with time."""
    def __init__(self, data=None, *, ttl):
        self.__mutex = Lock()
        if data is None:
            self.__data = {}
        else:
            self.__data = dict(data)
        self.__ttl = ttl

    def __cleanup(self):
        now = time.monotonic()
        for key, (timestamp, value) in list(self.__data.items()):
            if (timestamp + self.__ttl) <= now:
                del self.__data[key]

    def __getitem__(self, item):
        with self.__mutex:
            self.__cleanup()
            return self.__data[item][1]

    def __setitem__(self, key, value):
        with self.__mutex:
            self.__cleanup()
            self.__data[key] = (time.monotonic(), value)

    def __delitem__(self, key):
        with self.__mutex:
            self.__cleanup()
            del self.__data[key]

    def __iter__(self):
        with self.__mutex:
            self.__cleanup()
            return iter({k: v[1] for k, v in self.__data.items()})

    def __len__(self):
        with self.__mutex:
            self.__cleanup()
            return len(self.__data)
