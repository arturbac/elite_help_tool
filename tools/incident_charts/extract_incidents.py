#!/usr/bin/env python3
"""Finds the game's connection incidents in its own logs and writes them to a CSV file.

Reads two folders the game already keeps:
  - the netLog folder (<install>/Products/elite-dangerous-odyssey-64/Logs): checksum failure bursts and
    "Disconnect: type=N&reason=..." lines,
  - the journal folder (Saved Games/Frontier Developments/Elite Dangerous): sessions that ended without a
    Shutdown event.

The rules are the same as EHT's Network window (src/network_incident.cc), so a CSV exported from EHT's
live.sqlite and one made by this script can be plotted alike. Output columns: occurred_utc,category,detail.
Only the standard library is used.
"""

import argparse
import csv
import datetime as dt
import pathlib
import re
import sys
from zoneinfo import ZoneInfo

# checksum failures closer together than this are one incident
BURST_GAP = dt.timedelta(seconds=2)

LINE_TIME = re.compile(r"^\{(\d\d):(\d\d):(\d\d)GMT ")
NETLOG_NAME = re.compile(r"^netLog\.(\d{4}-\d\d-\d\dT\d{6})\.\d+\.log$")
JOURNAL_TIME = re.compile(r'"timestamp":"([0-9T:\-]+)Z"')
JOURNAL_EVENT = re.compile(r'"event":"([^"]*)"')


def read_text(path):
    return path.read_text(encoding="utf-8", errors="replace")


class NetlogClock:
    """A netLog line carries only the UTC time of day; the day comes from the file's name, which is the
    local time the file was opened at. Each line takes the day of the line before it, and a time of day
    more than 12 h behind the previous line means midnight has passed."""

    def __init__(self, file_name, zone):
        found = NETLOG_NAME.match(file_name)
        self.last = None
        if found:
            local = dt.datetime.strptime(found.group(1), "%Y-%m-%dT%H%M%S")
            local = local.replace(tzinfo=zone) if zone else local.astimezone()
            self.last = local.astimezone(dt.timezone.utc)

    def moment(self, line):
        found = LINE_TIME.match(line)
        if not found or self.last is None:
            return None
        h, m, s = (int(x) for x in found.groups())
        moment = self.last.replace(hour=h, minute=m, second=s, microsecond=0)
        if moment + dt.timedelta(hours=12) < self.last:
            moment += dt.timedelta(days=1)
        self.last = moment
        return moment


def scan_netlog(path, zone):
    clock = NetlogClock(path.name, zone)
    found = []
    burst_start = burst_last = None
    burst_count = 0
    burst_target = "?"

    def flush():
        nonlocal burst_start, burst_count
        if burst_start is not None:
            found.append((burst_start, "checksum failure", f"{burst_count} checksum failures against {burst_target}"))
        burst_start = None
        burst_count = 0

    for line in read_text(path).splitlines():
        moment = clock.moment(line)
        if moment is None:
            continue
        if "checksum failure" in line:
            if burst_start is not None and moment - burst_last > BURST_GAP:
                flush()
            if burst_start is None:
                burst_start = moment
                burst_target = "?"
                if "IP4:" in line:
                    burst_target = line.split("IP4:", 1)[1].split(",", 1)[0]
            burst_last = moment
            burst_count += 1
            continue
        if "Disconnect: type=" in line and "reason=" in line:
            reason = line.split("reason=", 1)[1].split("&", 1)[0]
            found.append((moment, f"disconnect: {reason}", line.strip()))
    flush()
    return found


def scan_journal(path):
    text = read_text(path)
    if '"event":"Shutdown"' in text:
        return None
    lines = [line for line in text.splitlines() if line.strip()]
    if not lines:
        return None
    last = lines[-1]
    # a journal grown too long is closed with Continued and goes on in the next part - that is no stop
    if '"event":"Continued"' in last:
        return None
    stamp = JOURNAL_TIME.search(last)
    if not stamp:
        return None
    moment = dt.datetime.strptime(stamp.group(1), "%Y-%m-%dT%H:%M:%S").replace(tzinfo=dt.timezone.utc)
    event = JOURNAL_EVENT.search(last)
    return (moment, "ended without Shutdown", f"last event before it stopped: {event.group(1) if event else '?'}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--netlog", type=pathlib.Path, help="the game's Logs folder holding netLog.*.log")
    parser.add_argument("--journal", type=pathlib.Path, help="the folder holding Journal.*.log")
    parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("incidents.csv"))
    parser.add_argument("--tz", help="time zone the game ran in, e.g. Europe/Warsaw; default: this machine's")
    parser.add_argument(
        "--include-newest-journal", action="store_true",
        help="also judge the newest journal - by default it is left out, as the game may still be writing it")
    args = parser.parse_args()
    if not args.netlog and not args.journal:
        parser.error("give --netlog, --journal or both")
    zone = ZoneInfo(args.tz) if args.tz else None

    rows = []
    if args.netlog:
        files = sorted(args.netlog.glob("netLog.*.log"))
        for path in files:
            rows += scan_netlog(path, zone)
        print(f"netLog: {len(files)} files", file=sys.stderr)
    if args.journal:
        files = sorted(args.journal.glob("Journal.*.log"))
        if files and not args.include_newest_journal:
            files = files[:-1]
        for path in files:
            if (row := scan_journal(path)) is not None:
                rows.append(row)
        print(f"journal: {len(files)} files", file=sys.stderr)

    rows.sort(key=lambda row: row[0])
    with args.out.open("w", newline="", encoding="utf-8") as out:
        writer = csv.writer(out)
        writer.writerow(["occurred_utc", "category", "detail"])
        for moment, category, detail in rows:
            writer.writerow([moment.strftime("%Y-%m-%dT%H:%M:%SZ"), category, detail])
    print(f"{len(rows)} incidents written to {args.out}", file=sys.stderr)


if __name__ == "__main__":
    main()
