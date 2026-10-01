#!/usr/bin/env python3
"""Draws three charts of connection incidents from a CSV of occurred_utc,category[,...] rows.

  incidents_daily.png    - stacked bars, one per day, of a chosen period (the last 120 days by default)
  incidents_history.png  - the whole history, all kinds summed: the daily count and its 7-day mean
  incidents_by_type.png  - the whole history, the 7-day mean of each kind
  incidents_per_hour.png - the whole history, a panel for each kind of trouble, the ones the game goes on
                           through among them: incidents per hour of play over 28 days (needs the
                           sessions.csv extract_incidents.py writes)

The CSV comes from extract_incidents.py, or from EHT's live.sqlite (see doc/incident_charts.md). Game
updates from game_updates.csv beside this script are marked on every chart. Needs matplotlib.
"""

import argparse
import collections
import csv
import datetime as dt
import pathlib
from zoneinfo import ZoneInfo

import matplotlib

matplotlib.use("Agg")
import matplotlib.dates as mdates  # noqa: E402
import matplotlib.pyplot as plt  # noqa: E402

KINDS = ["checksum failure", "disconnect", "ended without Shutdown"]
# what the game goes on through, as a rule - only on the per-hour chart
DEGRADED = ["server dropped", "packets lost", "player link dropped", "API failure", "API slow"]
COLOURS = {"checksum failure": "#2a78d6", "disconnect": "#1baf7a", "ended without Shutdown": "#eb6834",
           "server dropped": "#e34948", "packets lost": "#eda100", "player link dropped": "#e87ba4",
           "API failure": "#4a3aa7", "API slow": "#008300"}
PER_HOUR = ["disconnect", "server dropped", "packets lost", "player link dropped", "API failure", "API slow"]
TITLES = {"disconnect": "disconnect - the session lost",
          "server dropped": "server dropped - an EDServer given up, a disconnect follows 2 times in 3",
          "packets lost": "packets lost - from the game server, the game as a rule goes on",
          "player link dropped": "player link dropped - another player's link, not Frontier's",
          "API failure": "API failure - a burst of failed web requests (no answer, HTTP 5xx)",
          "API slow": "API slow - a burst of web requests of 10 s or more"}
ROLLING_DAYS = 28
GROUND = "#fcfcfb"
INK = "#0b0b0b"
MUTED = "#52514e"


def kind_of(category):
    """Every disconnect reason is one kind; older EHT databases call the journal kind "crash: no Shutdown"."""
    if category == "checksum failure":
        return category
    if category.startswith("disconnect"):
        return "disconnect"
    if category in DEGRADED:
        return category
    return "ended without Shutdown"


def load(path, zone):
    """(local date, kind) of every row - a day is the player's day, so the time is moved to local time."""
    rows = []
    with open(path, encoding="utf-8") as f:
        for row in csv.DictReader(f):
            moment = dt.datetime.strptime(row["occurred_utc"], "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=dt.timezone.utc)
            rows.append((moment.astimezone(zone).date(), kind_of(row["category"])))
    return rows


def load_updates(path):
    if not path.exists():
        return []
    with open(path, encoding="utf-8") as f:
        return [(dt.date.fromisoformat(row["date"]), row["label"]) for row in csv.DictReader(f)]


def days(start, end):
    return [start + dt.timedelta(days=n) for n in range((end - start).days + 1)]


def trailing_mean(values, window=7):
    """The mean of each day and the days before it; the first days average over what there is."""
    out = []
    for i in range(len(values)):
        part = values[max(0, i - window + 1) : i + 1]
        out.append(sum(part) / len(part))
    return out


def figure(width, height):
    fig, ax = plt.subplots(figsize=(width, height), dpi=150)
    fig.patch.set_facecolor(GROUND)
    ax.set_facecolor(GROUND)
    for spine in ("top", "right"):
        ax.spines[spine].set_visible(False)
    for spine in ("left", "bottom"):
        ax.spines[spine].set_color("#d8d6d0")
    ax.grid(axis="y", color="#e8e6e0", linewidth=0.8, zorder=0)
    ax.set_axisbelow(True)
    ax.tick_params(axis="y", colors=MUTED, labelsize=9)
    return fig, ax


def mark_updates(ax, updates, start, end, top):
    shown = [(date, label) for date, label in updates if start <= date <= end]
    for i, (date, label) in enumerate(shown):
        ax.axvline(date, color="#9a4a24", linestyle="--", linewidth=1.0, alpha=0.6, zorder=1)
        ax.annotate(label, xy=(date, 0), xytext=(date, top * (0.95 - (i % 3) * 0.09)), fontsize=7.5,
                    color="#6b3418", rotation=90, va="bottom", ha="right", annotation_clip=False)


def finish(fig, ax, title, ylabel, start, end, locator, fmt, out):
    ax.set_title(title, fontsize=13, color=INK, pad=14)
    ax.set_ylabel(ylabel, fontsize=10, color=MUTED)
    ax.xaxis.set_major_locator(locator)
    ax.xaxis.set_major_formatter(mdates.DateFormatter(fmt))
    plt.setp(ax.get_xticklabels(), rotation=45, ha="right", fontsize=8, color=MUTED)
    ax.set_xlim(start - dt.timedelta(days=1), end + dt.timedelta(days=1))
    ax.legend(loc="upper left", frameon=False, fontsize=9, labelcolor=INK)
    fig.tight_layout()
    fig.savefig(out, facecolor=fig.get_facecolor())
    plt.close(fig)
    print(f"written {out}")


def span(start, end):
    return f"{start:%b %d, %Y} - {end:%b %d, %Y}"


def daily_chart(rows, start, end, updates, out):
    dates = days(start, end)
    per_day = collections.defaultdict(collections.Counter)
    for date, kind in rows:
        if start <= date <= end:
            per_day[date][kind] += 1
    fig, ax = figure(16, 6)
    bottom = [0] * len(dates)
    for kind in KINDS:
        values = [per_day[d][kind] for d in dates]
        ax.bar(dates, values, bottom=bottom, width=1.0, color=COLOURS[kind], label=kind, linewidth=0)
        bottom = [b + v for b, v in zip(bottom, values)]
    mark_updates(ax, updates, start, end, max(bottom, default=1) or 1)
    finish(fig, ax, f"Elite Dangerous - incidents per day\n{span(start, end)}", "Incidents / day", start, end,
           mdates.WeekdayLocator(byweekday=mdates.MO), "%b %d", out)


def history_chart(rows, updates, out):
    start, end = min(d for d, _ in rows), max(d for d, _ in rows)
    dates = days(start, end)
    per_day = collections.Counter(d for d, _ in rows)
    counts = [per_day[d] for d in dates]
    fig, ax = figure(18, 7)
    ax.plot(dates, counts, color="#86b6ef", linewidth=0.9, alpha=0.8, label="incidents per day")
    ax.plot(dates, trailing_mean(counts), color="#2a78d6", linewidth=2.2, label="7-day mean")
    top = max(counts, default=1) or 1
    mark_updates(ax, updates, start, end, top)
    ax.set_ylim(0, top * 1.15)
    finish(fig, ax, f"Elite Dangerous - incidents per day, all kinds, with game updates\n{span(start, end)}",
           "Incidents / day", start, end, mdates.MonthLocator(), "%b %Y", out)


def by_type_chart(rows, updates, out):
    start, end = min(d for d, _ in rows), max(d for d, _ in rows)
    dates = days(start, end)
    per_day = collections.defaultdict(collections.Counter)
    for date, kind in rows:
        per_day[date][kind] += 1
    fig, ax = figure(18, 7)
    top = 0.0
    for kind in KINDS:
        mean = trailing_mean([per_day[d][kind] for d in dates])
        ax.plot(dates, mean, color=COLOURS[kind], linewidth=2.0, label=kind)
        top = max(top, max(mean, default=0))
    top = top or 1
    mark_updates(ax, updates, start, end, top)
    ax.set_ylim(0, top * 1.2)
    finish(fig, ax, f"Elite Dangerous - incidents by kind (7-day mean), with game updates\n{span(start, end)}",
           "Incidents / day (7-day mean)", start, end, mdates.MonthLocator(), "%b %Y", out)


def load_hours(path, zone):
    """Hours of play on each local day, from the sessions - one that runs past midnight is split."""
    hours = collections.Counter()
    if not path.exists():
        return hours
    with open(path, encoding="utf-8") as f:
        for row in csv.DictReader(f):
            start, end = (dt.datetime.strptime(row[k], "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=dt.timezone.utc)
                          .astimezone(zone) for k in ("start_utc", "end_utc"))
            while start < end:
                midnight = (start + dt.timedelta(days=1)).replace(hour=0, minute=0, second=0, microsecond=0)
                part_end = min(end, midnight)
                hours[start.date()] += (part_end - start).total_seconds() / 3600
                start = part_end
    return hours


def per_hour_chart(rows, hours, updates, out):
    # from the first day of play with a stretch of play behind it - a lone old netLog would stretch the axis
    start, end = min(d for d, _ in rows), max(d for d, _ in rows)
    while start < end and sum(hours[start + dt.timedelta(days=n)] for n in range(ROLLING_DAYS)) < 5:
        start += dt.timedelta(days=1)
    dates = days(start, end)
    per_day = collections.defaultdict(collections.Counter)
    for date, kind in rows:
        per_day[date][kind] += 1
    played = [hours[d] for d in dates]
    fig, axes = plt.subplots(len(PER_HOUR), 1, figsize=(18, 2.6 * len(PER_HOUR)), dpi=150, sharex=True)
    fig.patch.set_facecolor(GROUND)
    for ax, kind in zip(axes, PER_HOUR):
        ax.set_facecolor(GROUND)
        for spine in ("top", "right"):
            ax.spines[spine].set_visible(False)
        for spine in ("left", "bottom"):
            ax.spines[spine].set_color("#d8d6d0")
        ax.grid(axis="y", color="#e8e6e0", linewidth=0.8, zorder=0)
        ax.set_axisbelow(True)
        ax.tick_params(axis="y", colors=MUTED, labelsize=8)
        counts = [per_day[d][kind] for d in dates]
        rate = []
        for i in range(len(dates)):
            lo = max(0, i - ROLLING_DAYS + 1)
            h = sum(played[lo : i + 1])
            # fewer than 5 hours in the window says nothing of a rate
            rate.append(sum(counts[lo : i + 1]) / h if h >= 5 else float("nan"))
        ax.plot(dates, rate, color=COLOURS[kind], linewidth=2.0)
        total_h = sum(played)
        ax.set_title(f"{TITLES[kind]}   ({sum(counts)} in {total_h:.0f} h, {sum(counts) / total_h:.2f} / h in all)"
                     if total_h else TITLES[kind], fontsize=10, color=INK, loc="left")
        top = max((r for r in rate if r == r), default=0) or 1
        ax.set_ylim(0, top * 1.15)
        for date, label in updates:
            if start <= date <= end:
                ax.axvline(date, color="#9a4a24", linestyle="--", linewidth=1.0, alpha=0.5, zorder=1)
    for date, label in updates:
        if start <= date <= end:
            axes[0].annotate(label, xy=(date, 0.97), xycoords=("data", "axes fraction"), fontsize=7.5,
                             color="#6b3418", rotation=90, va="top", ha="right",
                             bbox={"facecolor": GROUND, "edgecolor": "none", "pad": 1})
    axes[-1].xaxis.set_major_locator(mdates.MonthLocator())
    axes[-1].xaxis.set_major_formatter(mdates.DateFormatter("%b %Y"))
    plt.setp(axes[-1].get_xticklabels(), rotation=45, ha="right", fontsize=8, color=MUTED)
    axes[-1].set_xlim(start - dt.timedelta(days=1), end + dt.timedelta(days=1))
    fig.suptitle(f"Elite Dangerous - incidents per hour the game ran ({ROLLING_DAYS}-day window), with game updates\n"
                 f"{span(start, end)}", fontsize=13, color=INK)
    fig.tight_layout()
    fig.savefig(out, facecolor=fig.get_facecolor())
    plt.close(fig)
    print(f"written {out}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("csv", type=pathlib.Path, help="incidents.csv")
    parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("."), help="folder for the PNG files")
    parser.add_argument("--tz", help="time zone of the player's day, e.g. Europe/Warsaw; default: this machine's")
    parser.add_argument("--from", dest="start", type=dt.date.fromisoformat, help="first day of the daily chart")
    parser.add_argument("--to", dest="end", type=dt.date.fromisoformat, help="last day of the daily chart")
    parser.add_argument("--sessions", type=pathlib.Path,
                        help="sessions.csv from extract_incidents.py; default: beside the incidents' CSV")
    parser.add_argument("--updates", type=pathlib.Path, default=pathlib.Path(__file__).with_name("game_updates.csv"))
    args = parser.parse_args()

    zone = ZoneInfo(args.tz) if args.tz else dt.datetime.now().astimezone().tzinfo
    rows = load(args.csv, zone)
    if not rows:
        raise SystemExit("no incidents in the file")
    updates = load_updates(args.updates)
    end = args.end or max(d for d, _ in rows)
    start = args.start or end - dt.timedelta(days=119)
    args.out.mkdir(parents=True, exist_ok=True)
    # the first three keep to the kinds they always showed - the trouble the game goes on through has a
    # chart of its own, where the hours played set the scale
    classic = [row for row in rows if row[1] in KINDS]
    daily_chart(classic, start, end, updates, args.out / "incidents_daily.png")
    history_chart(classic, updates, args.out / "incidents_history.png")
    by_type_chart(classic, updates, args.out / "incidents_by_type.png")
    hours = load_hours(args.sessions or args.csv.with_name("sessions.csv"), zone)
    if hours:
        per_hour_chart(rows, hours, updates, args.out / "incidents_per_hour.png")
    else:
        print("no sessions.csv - the per-hour chart is left out")


if __name__ == "__main__":
    main()
