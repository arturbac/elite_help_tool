# Inara: accept EDDN data from Elite Help Tool

Where to send:

- Inara forum thread "Inara updates, bug reports, requests" (author: Artie): https://inara.cz/elite/board-thread/1/460618/ (public, needs an Inara login)
- Inara Discord: https://discord.gg/qfkFWTr (private message possible)

## Message

**Subject:** Please accept EDDN fcmaterials_journal messages from "Elite Help Tool"

Hi Artie,

I'm the author of Elite Help Tool, a small desktop companion for Elite Dangerous (Linux, C++). Among other things it uploads to EDDN, following the EDDN developer guidelines and the same transformation rules as EDMC 6.1.2 (plugins/eddn.py).

Fleet carrier bar tender data sent by it does not show up on Inara, although EDDN accepts it and it goes out on the relay. I checked it this way on 27 Sep 2026:

- 21:51 UTC: my tool sent fcmaterials_journal/1 for my carrier. Inara still showed the bar tender as not updated six minutes later.
- 21:57 UTC: EDMC 6.1.2 sent fcmaterials_journal/1 for the same carrier with the same stock. The bar tender page updated within 30 seconds.

I compared both messages as they came out of the relay (tcp://eddn.edcd.io:9500). They carry the same 103 items with identical id/Name/Price/Stock/Demand, the same message fields (timestamp, event, horizons, odyssey, MarketID, CarrierName, CarrierID, Items) and the same gameversion/gamebuild. The only difference is the header's softwareName/softwareVersion.

So I guess Inara takes EDDN data only from known software. Would you consider adding it?

  softwareName: Elite Help Tool
  softwareVersion: 1.0.0 (will change with releases)

Schemas it sends: fcmaterials_journal/1, journal/1 (FSDJump, Scan, SAASignalsFound), fssdiscoveryscan/1, fssallbodiesfound/1, fssbodysignals/1, scanbarycentre/1 and codexentry/1. Exploration data is only sent for undiscovered, unpopulated systems and only after the cartographic data has been sold.

If there's anything you'd like changed on my side first, let me know.

Thanks for Inara!
CMDR Sjona Atreides
