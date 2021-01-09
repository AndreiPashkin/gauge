#!/usr/bin/env python
import argparse

import arrow
import requests

SONARCLOUD_WEB_API_BASE_URL = "https://sonarcloud.io"
PROJECT_KEY = "GAUGE"
ANALYSIS_TIME_TOLERANCE = 120


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        "get_sonar_results",
        description="""
Get analysis results from SonarCloud for a pull-request.
        """,
    )
    parser.add_argument(
        "--branch",
        "-B",
        required=True,
        help="Branch name.",
    )
    parser.add_argument(
        "--time",
        "-T",
        required=True,
        help="Analysis timestamp.",
        type=arrow.get,
    )
    parser.add_argument(
        "--commit",
        "-C",
        required=True,
        help="Commit.",
    )
    arguments = parser.parse_args()

    response = requests.get(
        f"{SONARCLOUD_WEB_API_BASE_URL}/api/project_pull_requests/list",
        params={"project": PROJECT_KEY},
    )
    if not response.ok:
        raise RuntimeError(response.content)

    min_timestamp = arguments.time.shift(seconds=ANALYSIS_TIME_TOLERANCE * -1)
    max_timestamp = arguments.time.shift(seconds=ANALYSIS_TIME_TOLERANCE)
    payload = response.json()
    matched_pull_request = None
    for pull_request in payload["pullRequests"]:
        if pull_request["branch"] != arguments.branch:
            continue
        if pull_request["commit"]["sha"] != arguments.commit:
            continue

        timestamp = arrow.get(pull_request["analysisDate"])
        if not (min_timestamp <= timestamp < max_timestamp):
            continue
        matched_pull_request = pull_request
        break
    if matched_pull_request is None:
        raise RuntimeError("No matching analysis results found.")

    if matched_pull_request["status"]["qualityGateStatus"].upper() == "OK":
        print("Ok.")
    else:
        raise RuntimeError(matched_pull_request["status"])
