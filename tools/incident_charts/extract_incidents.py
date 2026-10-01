#!/usr/bin/env python3
"""Finds the game's connection incidents in its own logs and writes them to a CSV file.

Reads two folders the game already keeps:
  - the netLog folder (<install>/Products/elite-dangerous-odyssey-64/Logs): checksum failure bursts and
    "Disconnect: type=N&reason=..." lines, and the trouble that does not always end in a disconnect -
    packets lost from the game server, a server dropped after too many retries, and bursts of failed or
    slow requests to the web API beside it (the same lines EHT's overlay warns of while they happen),
  - the journal folder (Saved Games/Frontier Developments/Elite Dangerous): sessions that ended without a
    Shutdown event.

The rules are the same as EHT's Network window (src/network_incident.cc), so a CSV exported from EHT's
live.sqlite and one made by this script can be plotted alike. Output columns: occurred_utc,category,detail.
With --netlog, the sessions are written too (--sessions, start_utc,end_utc - each netLog file's first and
last line), so the charts can count incidents per hour of play rather than per day.
Only the standard library is used.
"""

import argparse
import collections
import csv
import datetime as dt
import pathlib
import re
import sys
from zoneinfo import ZoneInfo

# checksum failures closer together than this are one incident
BURST_GAP = dt.timedelta(seconds=2)
# lost packets, failed and slow web requests closer together than this are one episode
EPISODE_GAP = dt.timedelta(seconds=60)

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


class Episodes:
    """Lines of one kind closer together than EPISODE_GAP make one row: its first moment and how many."""

    def __init__(self, category, found):
        self.category = category
        self.found = found
        self.start = self.last = None
        self.count = 0
        self.notes = collections.Counter()

    def add(self, moment, note):
        if self.start is not None and moment - self.last > EPISODE_GAP:
            self.flush()
        if self.start is None:
            self.start = moment
        self.last = moment
        self.count += 1
        self.notes[note] += 1

    def flush(self):
        if self.start is not None:
            notes = ", ".join(f"{n}x {note}" for note, n in self.notes.most_common())
            self.found.append((self.start, self.category, f"{self.count} in {(self.last - self.start).seconds} s: {notes}"))
        self.start = None
        self.count = 0
        self.notes = collections.Counter()


def scan_netlog(path, zone):
    """The incidents of one file, and its session: the first and the last moment of its lines."""
    clock = NetlogClock(path.name, zone)
    found = []
    first = last = None
    lost = Episodes("packets lost", found)
    api_failed = Episodes("API failure", found)
    api_slow = Episodes("API slow", found)
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
        first = first or moment
        last = moment
        if "Several LOST packet#" in line:
            server = re.search(r"EDServer#\d+", line)
            lost.add(moment, server.group(0) if server else "?")
            continue
        if "} Disconnected: " in line and "(Too many retries)" in line and "EDServer#" in line:
            found.append((moment, "server dropped", re.search(r"EDServer#\d+", line).group(0)))
            continue
        # the link to another player - no word of Frontier's servers; leaving in good order is no trouble
        if "} Disconnected: " in line and "EDServer#" not in line and "ThisMachine" not in line \
                and "(shutdown)" not in line:
            reason = line.rsplit("(", 1)[-1].rstrip(")\n ")
            found.append((moment, "player link dropped", ("relay, " if "((Relay))" in line else "") + reason))
            continue
        if "Webserver request failed: code " in line:
            code = line.split("failed: code ", 1)[1].split(",", 1)[0]
            api_failed.add(moment, "no answer" if code == "0" else f"HTTP {code}")
            continue
        if "HTTP Request took " in line:
            what = line.split("/2.0/elite/", 1)[1] if "/2.0/elite/" in line else line.split("complete: ", 1)[-1]
            api_slow.add(moment, what.split("?", 1)[0].strip())
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
    for episodes in (lost, api_failed, api_slow):
        episodes.flush()
    return found, (first, last) if first else None


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
    parser.add_argument("--sessions", type=pathlib.Path, default=pathlib.Path("sessions.csv"),
                        help="where the sessions read from netLog go (start_utc,end_utc)")
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
        sessions = []
        for path in files:
            found, session = scan_netlog(path, zone)
            rows += found
            if session:
                sessions.append(session)
        print(f"netLog: {len(files)} files", file=sys.stderr)
        with args.sessions.open("w", newline="", encoding="utf-8") as out:
            writer = csv.writer(out)
            writer.writerow(["start_utc", "end_utc"])
            for start, end in sessions:
                writer.writerow([start.strftime("%Y-%m-%dT%H:%M:%SZ"), end.strftime("%Y-%m-%dT%H:%M:%SZ")])
        print(f"{len(sessions)} sessions written to {args.sessions}", file=sys.stderr)
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
