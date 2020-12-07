#!/usr/bin/env python
import argparse
import os
import subprocess
import sys
import difflib


DIRECTORIES = [
    os.path.join(
        os.path.dirname(os.path.abspath(__file__)), os.pardir, "include"
    ),
    os.path.join(
        os.path.dirname(os.path.abspath(__file__)), os.pardir, "src", "cpp"
    ),
]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        "format",
        description="""
Format C++ sources using Clang-Format.
        """,
    )
    parser.add_argument(
        "--check",
        "-C",
        help="Do not format files but check if formatting is needed and exit "
        "with non-zero code if it is.",
        action="store_true",
        default=False,
    )
    parser.add_argument(
        "files",
        nargs="*",
    )
    arguments = parser.parse_args()
    filepaths = []
    if arguments.files:
        filepaths.extend(arguments.files)
    else:
        filepaths = [
            os.path.join(p, f)
            for d in DIRECTORIES
            for p, _, fs in os.walk(d, followlinks=True)
            for f in fs
        ]
    if arguments.check:
        for filepath in filepaths:
            result = subprocess.run(
                ["clang-format", "-style=file", filepath],
                capture_output=True,
                env=os.environ,
            )
            if result.returncode != 0:
                sys.exit(result.returncode)
            with open(filepath) as file_handle:
                actual_source = file_handle.read()
            diff = "\n".join(
                difflib.unified_diff(
                    result.stdout.decode().split("\n"),
                    actual_source.split("\n"),
                    "Formatted source",
                    f"Actual source ({filepath})",
                    lineterm="",
                )
            )
            if diff:
                print("Fail")
                print(diff, file=sys.stderr, flush=True)
                sys.exit(1)
        print("Ok")
        sys.exit(0)
    else:
        result = subprocess.run(
            ["clang-format", "-i", "-style=file"] + filepaths, env=os.environ
        )
        sys.exit(result.returncode)
