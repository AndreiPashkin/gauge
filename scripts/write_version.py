#!/usr/bin/env python
import argparse
import os
import re
from itertools import zip_longest

import dunamai


SEMVER_PATTERN = (
    r"^(?P<base>(?P<major>0|[1-9]\d*)\.(?P<minor>0|[1-9]\d*)\."
    r"(?P<patch>0|[1-9]\d*))"
    r"(?:-(?P<stage>(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)"
    r"(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?"
    r"(?:\+(?P<revision>[0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$"
)
VERSION_FILE = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "VERSION"
)


def from_file(file, pattern=SEMVER_PATTERN):
    with open(file) as version_file:
        version_string = version_file.read()
        match = re.match(pattern, version_string)
        if not match:
            raise ValueError
        match_dict = match.groupdict()
        return dunamai.Version(
            base=match_dict["base"],
            stage=match_dict.get("stage"),
            commit=match_dict.get("revision"),
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser("write-version")
    parser.add_argument(
        "-M", "--upgrade-major", action="store_true", default=False
    )
    parser.add_argument(
        "-m", "--upgrade-minor", action="store_true", default=False
    )
    parser.add_argument(
        "-p", "--upgrade-patch", action="store_true", default=False
    )
    arguments = parser.parse_args()
    if (
        arguments.upgrade_major
        or arguments.upgrade_minor
        or arguments.upgrade_patch
    ):
        version = dunamai.get_version(
            "gauge", first_choice=lambda: from_file(file=VERSION_FILE)
        )
    else:
        version = dunamai.get_version(
            "gauge",
            first_choice=lambda: dunamai.Version.from_any_vcs(
                pattern=SEMVER_PATTERN
            ),
        )
    base = dict(
        zip_longest(
            ("major", "minor", "patch"),
            map(int, version.base.split(".")),
            fillvalue=0,
        )
    )
    if arguments.upgrade_major:
        base["major"] += 1
    if arguments.upgrade_minor:
        base["minor"] += 1
    if arguments.upgrade_patch:
        base["patch"] += 1

    base_string = f'{base["major"]}.{base["minor"]}.{base["patch"]}'
    version.base = base_string
    version_string = version.serialize(style=dunamai.Style.SemVer)

    with open("VERSION", "w") as version_file:
        version_file.write(version_string)
