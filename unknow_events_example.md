# Nieznane eventy journala - material do przetworzenia

Lista eventow, ktorych `elite_help_tool` nie obsluguje, zebrana z pelnego korpusu journali.
**Nic z tego nie jest zaimplementowane** - plik jest wylacznie materialem na pozniej.

| | |
|---|---|
| data skanu | 2026-09-25 |
| przeskanowane pliki | 1757 journali (1.8 GB) |
| zakres czasu | 2025-11-17 .. 2026-09-24 |
| typow eventow w journalach | 231 |
| wpisow w `events::event_e` | 145 |
| **typow bez obslugi** | **86** (13665 wystapien) |

Przyklady to surowe linie z journali, skrocone: tablice dluzsze niz 2 elementy sa przyciete.

---

## 1. Eventy spoza `events::event_e`

`enum_cast<event_e>` nie rozpoznaje nazwy, linia konczy na `failed to cast event type` i jest pomijana.
Posortowane malejaco wg liczby wystapien - gora listy to kandydaci do implementacji w pierwszej kolejnosci.

Jeden wyjatek: `ScanOrganic` nie ma wpisu w enumie, ale w logu nie pojawia sie **ani razu** jako
`failed to cast` - jego linie gina wczesniej, na parsowaniu `generic_event_t` (patrz sekcja 2).

| event | wystapien |
|---|---:|
| `CommunityGoal` | 5266 |
| `ScanOrganic` | 1292 |
| `ProspectedAsteroid` | 1157 |
| `DropshipDeploy` | 783 |
| `DropItems` | 744 |
| `CarrierTradeOrder` | 665 |
| `BookDropship` | 627 |
| `SendText` | 433 |
| `WingAdd` | 308 |
| `CarrierBankTransfer` | 200 |
| `Interdiction` | 168 |
| `WingLeave` | 133 |
| `DatalinkScan` | 129 |
| `CarrierDepositFuel` | 111 |
| `DatalinkVoucher` | 104 |
| `SharedBookmarkToSquadron` | 94 |
| `Resupply` | 92 |
| `UpgradeWeapon` | 88 |
| `CarrierJump` | 75 |
| `WingJoin` | 71 |
| `FighterDestroyed` | 70 |
| `FighterRebuilt` | 70 |
| `CarrierFinance` | 68 |
| `CarrierCrewServices` | 63 |
| `Synthesis` | 57 |
| `SellOrganicData` | 57 |
| `WingInvite` | 51 |
| `AfmuRepairs` | 48 |
| `LaunchVessel` | 45 |
| `ModuleSwap` | 45 |
| `UpgradeSuit` | 40 |
| `ModuleSell` | 39 |
| `CrewHire` | 38 |
| `CrewFire` | 37 |
| `MarketID` | 33 |
| `SetUserShipName` | 31 |
| `EngineerContribution` | 27 |
| `BuyExplorationData` | 26 |
| `CarrierDockingPermission` | 26 |
| `ColonisationSystemClaim` | 24 |
| `ColonisationBeaconDeployed` | 24 |
| `ShipyardBankDeposit` | 17 |
| `CarrierJumpCancelled` | 16 |
| `NpcCrewRank` | 13 |
| `RenameSuitLoadout` | 12 |
| `DeleteSuitLoadout` | 12 |
| `Screenshot` | 11 |
| `CommunityGoalJoin` | 11 |
| `TechnologyBroker` | 10 |
| `DataScanned` | 9 |
| `QuitACrew` | 8 |
| `SellSuit` | 7 |
| `CancelDropship` | 6 |
| `VehicleSwitch` | 6 |
| `ShipyardRedeem` | 5 |
| `ShipRedeemed` | 5 |
| `SellWeapon` | 5 |
| `CommunityGoalDiscard` | 4 |
| `JoinedSquadron` | 4 |
| `Powerplay` | 4 |
| `LoadoutRemoveModule` | 4 |
| `ShipyardSell` | 3 |
| `ColonisationSystemClaimRelease` | 3 |
| `CarrierBuy` | 3 |
| `AppliedToSquadron` | 2 |
| `SquadronCreated` | 2 |
| `DockingTimeout` | 2 |
| `EndCrewSession` | 2 |
| `CarrierNameChange` | 2 |
| `CockpitBreached` | 2 |
| `LeftSquadron` | 1 |
| `SelfDestruct` | 1 |
| `PowerplayLeave` | 1 |
| `PowerplayRank` | 1 |
| `PowerplayJoin` | 1 |
| `PowerplayMerits` | 1 |
| `NewCommander` | 1 |
| `CrewMemberJoins` | 1 |
| `CrewMemberRoleChange` | 1 |
| `CrewMemberQuits` | 1 |
| `ChangeCrewRole` | 1 |
| `RepairDrone` | 1 |
| `CommunityGoalReward` | 1 |
| `CompleteConstruction` | 1 |
| `RebootRepair` | 1 |
| `BuyTradeData` | 1 |

### Przyklady

#### CommunityGoal

*5266 wystapien*

```json
{
  "timestamp": "2026-08-15T13:20:52Z",
  "event": "CommunityGoal",
  "CurrentGoals": [
    {
      "CGID": 852,
      "Title": "Asura Calls for Assistance to Distribute Celebratory Commodities",
      "SystemName": "Asura",
      "MarketName": "Mizuno Dock",
      "Expiry": "2026-08-06T10:00:00Z",
      "IsComplete": true,
      "CurrentTotal": 11500663,
      "PlayerContribution": 17,
      "NumContributors": 5082,
      "TopTier": {
        "Name": "Tier 1",
        "Bonus": ""
      },
      "TopRankSize": 10,
      "PlayerInTopRank": false,
      "TierReached": "Tier 1",
      "PlayerPercentileBand": 100,
      "Bonus": 13750000
    },
    {
      "CGID": 853,
      "Title": "Randgnid Calls for Assistance to Distribute Celebratory Commodities",
      "SystemName": "Randgnid",
      "MarketName": "Templar Barracks",
      "Expiry": "2026-08-06T10:00:00Z",
      "IsComplete": true,
      "CurrentTotal": 23000121,
      "PlayerContribution": 16,
      "NumContributors": 7848,
      "TopTier": {
        "Name": "Tier 1",
        "Bonus": ""
      },
      "TopRankSize": 10,
      "PlayerInTopRank": false,
      "TierReached": "Tier 1",
      "PlayerPercentileBand": 100,
      "Bonus": 13750000
    },
    "... (4 elementow lacznie)"
  ]
}
```

#### ScanOrganic

*1292 wystapien*

```json
{
  "timestamp": "2026-09-21T21:34:56Z",
  "event": "ScanOrganic",
  "ScanType": "Log",
  "Genus": "$Codex_Ent_Bacterial_Genus_Name;",
  "Genus_Localised": "Bacterium",
  "Species": "$Codex_Ent_Bacterial_04_Name;",
  "Species_Localised": "Bacterium Acies",
  "Variant": "$Codex_Ent_Bacterial_04_Tellurium_Name;",
  "Variant_Localised": "Bacterium Acies - White",
  "WasLogged": false,
  "SystemAddress": 22933514506792,
  "Body": 12
}
```

#### ProspectedAsteroid

*1157 wystapien*

```json
{
  "timestamp": "2026-08-06T22:31:23Z",
  "event": "ProspectedAsteroid",
  "Materials": [
    {
      "Name": "MethanolMonohydrateCrystals",
      "Name_Localised": "Methanol Monohydrate Crystals",
      "Proportion": 18.613247
    },
    {
      "Name": "MethaneClathrate",
      "Name_Localised": "Methane Clathrate",
      "Proportion": 6.634248
    },
    "... (3 elementow lacznie)"
  ],
  "Content": "$AsteroidMaterialContent_Medium;",
  "Content_Localised": "Material Content: Medium",
  "Remaining": 100.0
}
```

#### DropshipDeploy

*783 wystapien*

```json
{
  "timestamp": "2026-09-24T01:09:52Z",
  "event": "DropshipDeploy",
  "StarSystem": "Bleia Eohn LQ-F b31-7",
  "SystemAddress": 16060895865097,
  "Body": "Bleia Eohn LQ-F b31-7 A 5",
  "BodyID": 13,
  "OnStation": false,
  "OnPlanet": true
}
```

#### DropItems

*744 wystapien*

```json
{
  "timestamp": "2026-09-23T14:56:27Z",
  "event": "DropItems",
  "Name": "insight",
  "Type": "Item",
  "OwnerID": 2953052717,
  "Count": 1
}
```

#### CarrierTradeOrder

*665 wystapien*

```json
{
  "timestamp": "2026-09-20T22:11:52Z",
  "event": "CarrierTradeOrder",
  "CarrierID": 3706381824,
  "CarrierType": "FleetCarrier",
  "BlackMarket": false,
  "Commodity": "biometricdata",
  "Commodity_Localised": "Biometric Data",
  "SaleOrder": 12,
  "Price": 2388000
}
```

#### BookDropship

*627 wystapien*

```json
{
  "timestamp": "2026-09-23T16:45:42Z",
  "event": "BookDropship",
  "Retreat": false,
  "Cost": 0,
  "DestinationSystem": "Bleia Eohn LQ-F b31-7",
  "DestinationLocation": "Onishi's Armoury"
}
```

#### SendText

*433 wystapien*

```json
{
  "timestamp": "2026-09-18T20:54:49Z",
  "event": "SendText",
  "To": "Firewall",
  "Message": "Hey. Thank You for scanning system, I owe You one.",
  "Sent": true
}
```

#### WingAdd

*308 wystapien*

```json
{
  "timestamp": "2026-09-19T09:53:07Z",
  "event": "WingAdd",
  "Name": "LEGUNZOO"
}
```

#### CarrierBankTransfer

*200 wystapien*

```json
{
  "timestamp": "2026-09-23T02:04:11Z",
  "event": "CarrierBankTransfer",
  "CarrierID": 3706381824,
  "CarrierType": "FleetCarrier",
  "Withdraw": 225100480,
  "PlayerBalance": 40477583488,
  "CarrierBalance": 5000000000
}
```

#### Interdiction

*168 wystapien*

```json
{
  "timestamp": "2026-09-19T19:39:13Z",
  "event": "Interdiction",
  "Success": true,
  "IsPlayer": false,
  "Faction": "Cartel of HIP 83983"
}
```

#### WingLeave

*133 wystapien*

```json
{
  "timestamp": "2026-09-19T09:52:34Z",
  "event": "WingLeave"
}
```

#### DatalinkScan

*129 wystapien*

```json
{
  "timestamp": "2026-06-22T09:07:56Z",
  "event": "DatalinkScan",
  "Message": "$DataLink_ScannedInfo_Security;",
  "Message_Localised": "Security grid accessed"
}
```

#### CarrierDepositFuel

*111 wystapien*

```json
{
  "timestamp": "2026-09-24T06:07:53Z",
  "event": "CarrierDepositFuel",
  "CarrierID": 3715010304,
  "Amount": 7,
  "Total": 769
}
```

#### DatalinkVoucher

*104 wystapien*

```json
{
  "timestamp": "2026-04-30T19:06:54Z",
  "event": "DatalinkVoucher",
  "Reward": 3629,
  "VictimFaction": "",
  "PayeeFaction": "Empire"
}
```

#### SharedBookmarkToSquadron

*94 wystapien*

```json
{
  "timestamp": "2026-09-22T14:04:24Z",
  "event": "SharedBookmarkToSquadron",
  "SquadronID": 111649,
  "SquadronName": "CORKI NALOZNICY SZATANA"
}
```

#### Resupply

*92 wystapien*

```json
{
  "timestamp": "2026-09-20T00:32:08Z",
  "event": "Resupply"
}
```

#### UpgradeWeapon

*88 wystapien*

```json
{
  "timestamp": "2026-08-14T13:48:50Z",
  "event": "UpgradeWeapon",
  "Name": "wpn_m_assaultrifle_laser_fauto",
  "Name_Localised": "TK Aphelion",
  "Class": 2,
  "SuitModuleID": 1873440900943793,
  "Cost": 500000,
  "Resources": [
    {
      "Name": "weaponschematic",
      "Name_Localised": "Weapon Schematic",
      "Count": 1
    },
    {
      "Name": "ionisedgas",
      "Name_Localised": "Ionised Gas",
      "Count": 1
    },
    "... (5 elementow lacznie)"
  ]
}
```

#### CarrierJump

*75 wystapien*

```json
{
  "timestamp": "2026-08-19T05:44:13Z",
  "event": "CarrierJump",
  "Docked": true,
  "StationName": "PRNH",
  "StationType": "FleetCarrier",
  "MarketID": 3715010304,
  "StationFaction": {
    "Name": "FleetCarrier"
  },
  "StationGovernment": "$government_Carrier;",
  "StationGovernment_Localised": "Private Ownership",
  "StationServices": [
    "dock",
    "autodock",
    "... (17 elementow lacznie)"
  ],
  "StationEconomy": "$economy_Carrier;",
  "StationEconomy_Localised": "Private Enterprise",
  "StationEconomies": [
    {
      "Name": "$economy_Carrier;",
      "Name_Localised": "Private Enterprise",
      "Proportion": 1.0
    }
  ],
  "Taxi": false,
  "Multicrew": false,
  "StarSystem": "Bleia Eohn PT-O d7-21",
  "SystemAddress": 731981531715,
  "StarPos": [
    -232.4375,
    -31.3125,
    "... (3 elementow lacznie)"
  ],
  "SystemAllegiance": "Independent",
  "SystemEconomy": "$economy_Agri;",
  "SystemEconomy_Localised": "Agriculture",
  "SystemSecondEconomy": "$economy_Refinery;",
  "SystemSecondEconomy_Localised": "Refinery",
  "SystemGovernment": "$government_Feudal;",
  "SystemGovernment_Localised": "Feudal",
  "SystemSecurity": "$SYSTEM_SECURITY_low;",
  "SystemSecurity_Localised": "Low Security",
  "Population": 143565743,
  "Body": "Bleia Eohn PT-O d7-21 AB 3 c",
  "BodyID": 37,
  "BodyType": "Planet",
  "Factions": [
    {
      "Name": "The Dukes of Mikunn",
      "FactionState": "None",
      "Government": "Feudal",
      "Influence": 0.616384,
      "Allegiance": "Independent",
      "Happiness": "$Faction_HappinessBand2;",
      "Happiness_Localised": "Happy",
      "MyReputation": 100.0,
      "RecoveringStates": [
        {
          "State": "Expansion",
          "Trend": 0
        }
      ]
    },
    {
      "Name": "The Mercs of Mikunn",
      "FactionState": "Expansion",
      "Government": "Dictatorship",
      "Influence": 0.206793,
      "Allegiance": "Independent",
      "Happiness": "$Faction_HappinessBand2;",
      "Happiness_Localised": "Happy",
      "MyReputation": -7.9692,
      "ActiveStates": [
        {
          "State": "Expansion"
        }
      ]
    },
    "... (4 elementow lacznie)"
  ],
  "SystemFaction": {
    "Name": "The Dukes of Mikunn"
  }
}
```

#### WingJoin

*71 wystapien*

```json
{
  "timestamp": "2026-09-19T09:51:32Z",
  "event": "WingJoin",
  "Others": []
}
```

#### FighterDestroyed

*70 wystapien*

```json
{
  "timestamp": "2026-09-06T20:04:35Z",
  "event": "FighterDestroyed",
  "ID": 61
}
```

#### FighterRebuilt

*70 wystapien*

```json
{
  "timestamp": "2026-09-06T20:06:05Z",
  "event": "FighterRebuilt",
  "Loadout": "two",
  "ID": 61
}
```

#### CarrierFinance

*68 wystapien*

```json
{
  "timestamp": "2026-08-02T10:35:54Z",
  "event": "CarrierFinance",
  "CarrierID": 3708677888,
  "CarrierType": "FleetCarrier",
  "CarrierBalance": 3671168989,
  "ReserveBalance": 1101350696,
  "AvailableBalance": 2469372293,
  "ReservePercent": 30,
  "TaxRate_rearm": 0,
  "TaxRate_refuel": 0,
  "TaxRate_repair": 0
}
```

#### CarrierCrewServices

*63 wystapien*

```json
{
  "timestamp": "2026-09-03T18:53:56Z",
  "event": "CarrierCrewServices",
  "CarrierID": 3715010304,
  "CarrierType": "SquadronCarrier",
  "CrewRole": "Rearm",
  "Operation": "Pause",
  "CrewName": "Nancy Avila"
}
```

#### Synthesis

*57 wystapien*

```json
{
  "timestamp": "2026-09-24T05:37:30Z",
  "event": "Synthesis",
  "Name": "Chaff Basic",
  "Materials": [
    {
      "Name": "compactcomposites",
      "Name_Localised": "Compact Composites",
      "Count": 1
    },
    {
      "Name": "filamentcomposites",
      "Name_Localised": "Filament Composites",
      "Count": 1
    }
  ]
}
```

#### SellOrganicData

*57 wystapien*

```json
{
  "timestamp": "2026-09-22T02:11:30Z",
  "event": "SellOrganicData",
  "MarketID": 4308724739,
  "BioData": [
    {
      "Genus": "$Codex_Ent_Fungoids_Genus_Name;",
      "Genus_Localised": "Fungoida",
      "Species": "$Codex_Ent_Fungoids_03_Name;",
      "Species_Localised": "Fungoida Bullarum",
      "Variant": "$Codex_Ent_Fungoids_03_Technetium_Name;",
      "Variant_Localised": "Fungoida Bullarum - Peach",
      "Value": 3703200,
      "Bonus": 14812800
    },
    {
      "Genus": "$Codex_Ent_Osseus_Genus_Name;",
      "Genus_Localised": "Osseus",
      "Species": "$Codex_Ent_Osseus_04_Name;",
      "Species_Localised": "Osseus Pumice",
      "Variant": "$Codex_Ent_Osseus_04_Technetium_Name;",
      "Variant_Localised": "Osseus Pumice - Lime",
      "Value": 3156300,
      "Bonus": 12625200
    }
  ]
}
```

#### WingInvite

*51 wystapien*

```json
{
  "timestamp": "2026-08-19T12:52:09Z",
  "event": "WingInvite",
  "Name": "Dripnificent"
}
```

#### AfmuRepairs

*48 wystapien*

```json
{
  "timestamp": "2026-08-06T05:01:50Z",
  "event": "AfmuRepairs",
  "Module": "$int_hyperdrive_overcharge_size8_class5_overchargebooster_mkii_name;",
  "Module_Localised": "FSD (SCO)",
  "FullyRepaired": true,
  "Health": 1.0
}
```

#### LaunchVessel

*45 wystapien*

```json
{
  "timestamp": "2026-09-15T02:59:41Z",
  "event": "LaunchVessel",
  "VesselType": "lander01",
  "VesselType_Localised": "Nomad",
  "Loadout": "base",
  "ID": 30,
  "PlayerControlled": true
}
```

#### ModuleSwap

*45 wystapien*

```json
{
  "timestamp": "2026-08-22T05:23:16Z",
  "event": "ModuleSwap",
  "MarketID": 4248525315,
  "FromSlot": "Slot01_Size6",
  "ToSlot": "Slot02_Size5",
  "FromItem": "$int_cargorack_size5_class1_name;",
  "FromItem_Localised": "Cargo Rack",
  "ToItem": "Null",
  "Ship": "mandalay",
  "ShipID": 63
}
```

#### UpgradeSuit

*40 wystapien*

```json
{
  "timestamp": "2026-08-14T13:47:54Z",
  "event": "UpgradeSuit",
  "Name": "tacticalsuit_class2",
  "Name_Localised": "$TacticalSuit_Class1_Name;",
  "SuitID": 1871427198999012,
  "Class": 3,
  "Cost": 2250000,
  "Resources": [
    {
      "Name": "suitschematic",
      "Name_Localised": "Suit Schematic",
      "Count": 2
    },
    {
      "Name": "healthmonitor",
      "Name_Localised": "Health Monitor",
      "Count": 2
    },
    "... (5 elementow lacznie)"
  ]
}
```

#### ModuleSell

*39 wystapien*

```json
{
  "timestamp": "2026-09-13T07:07:15Z",
  "event": "ModuleSell",
  "MarketID": 4379304195,
  "Slot": "SmallHardpoint1",
  "SellItem": "$hpt_pulselaser_gimbal_small_name;",
  "SellItem_Localised": "Pulse Laser",
  "SellPrice": 6435,
  "Ship": "sidewinder",
  "ShipID": 48
}
```

#### CrewHire

*38 wystapien*

```json
{
  "timestamp": "2026-06-20T22:37:28Z",
  "event": "CrewHire",
  "Name": "Robin Hanson",
  "CrewID": 134427616,
  "Faction": "Cartel of HIP 83983",
  "Cost": 15000,
  "CombatRank": 0
}
```

#### CrewFire

*37 wystapien*

```json
{
  "timestamp": "2026-08-01T12:13:41Z",
  "event": "CrewFire",
  "Name": "Robin Hanson",
  "CrewID": 134427616
}
```

#### MarketID

*33 wystapien*

```json
{
  "timestamp": "2026-09-19T09:51:51Z",
  "event": "MarketID"
}
```

#### SetUserShipName

*31 wystapien*

```json
{
  "timestamp": "2026-08-22T08:03:51Z",
  "event": "SetUserShipName",
  "Ship": "mandalay",
  "ShipID": 63,
  "UserShipName": "[ms] Taxi 3",
  "UserShipId": "SJ-22M"
}
```

#### EngineerContribution

*27 wystapien*

```json
{
  "timestamp": "2026-08-06T13:59:40Z",
  "event": "EngineerContribution",
  "Engineer": "Mel Brandon",
  "EngineerID": 300280,
  "Type": "Bounty",
  "Quantity": 95400,
  "TotalQuantity": 95400
}
```

#### BuyExplorationData

*26 wystapien*

```json
{
  "timestamp": "2026-08-31T05:04:29Z",
  "event": "BuyExplorationData",
  "System": "Bleia Eohn TQ-L a62-1",
  "Cost": 0
}
```

#### CarrierDockingPermission

*26 wystapien*

```json
{
  "timestamp": "2026-08-08T23:40:51Z",
  "event": "CarrierDockingPermission",
  "CarrierID": 3706381824,
  "CarrierType": "FleetCarrier",
  "DockingAccess": "all",
  "AllowNotorious": true
}
```

#### ColonisationSystemClaim

*24 wystapien*

```json
{
  "timestamp": "2026-09-18T12:28:46Z",
  "event": "ColonisationSystemClaim",
  "StarSystem": "Bleia Eohn PW-D b32-1",
  "SystemAddress": 2866756331793
}
```

#### ColonisationBeaconDeployed

*24 wystapien*

```json
{
  "timestamp": "2026-09-18T12:31:35Z",
  "event": "ColonisationBeaconDeployed"
}
```

#### ShipyardBankDeposit

*17 wystapien*

```json
{
  "timestamp": "2026-08-01T12:05:45Z",
  "event": "ShipyardBankDeposit",
  "ShipType": "lakonminer",
  "ShipType_Localised": "Type-11 Prospector",
  "MarketID": 3715010304
}
```

#### CarrierJumpCancelled

*16 wystapien*

```json
{
  "timestamp": "2026-08-27T05:00:58Z",
  "event": "CarrierJumpCancelled",
  "CarrierType": "SquadronCarrier",
  "CarrierID": 3715010304
}
```

#### NpcCrewRank

*13 wystapien*

```json
{
  "timestamp": "2026-09-19T17:38:19Z",
  "event": "NpcCrewRank",
  "NpcCrewName": "Adele Saunders",
  "NpcCrewId": 110593536,
  "RankCombat": 5
}
```

#### RenameSuitLoadout

*12 wystapien*

```json
{
  "timestamp": "2026-09-05T16:45:33Z",
  "event": "RenameSuitLoadout",
  "SuitID": 1869106117286565,
  "SuitName": "utilitysuit_class5",
  "SuitName_Localised": "$UtilitySuit_Class1_Name;",
  "LoadoutID": 4293000011,
  "LoadoutName": "[DJA] Heist Riffle"
}
```

#### DeleteSuitLoadout

*12 wystapien*

```json
{
  "timestamp": "2026-08-14T23:14:44Z",
  "event": "DeleteSuitLoadout",
  "SuitID": 1866434097310163,
  "SuitName": "utilitysuit_class5",
  "SuitName_Localised": "$UtilitySuit_Class1_Name;",
  "LoadoutID": 4293000001,
  "LoadoutName": "[n] HEIST"
}
```

#### Screenshot

*11 wystapien*

```json
{
  "timestamp": "2026-09-02T23:24:02Z",
  "event": "Screenshot",
  "Filename": "\\ED_Pictures\\Screenshot_0010.bmp",
  "Width": 9000,
  "Height": 2160,
  "System": "Eoch Pruae ZK-E d12-986",
  "Body": "Eoch Pruae ZK-E d12-986 2 c"
}
```

#### CommunityGoalJoin

*11 wystapien*

```json
{
  "timestamp": "2026-07-30T19:55:10Z",
  "event": "CommunityGoalJoin",
  "CGID": 852,
  "Name": "Asura Calls for Assistance to Distribute Celebratory Commodities",
  "System": "Asura"
}
```

#### TechnologyBroker

*10 wystapien*

```json
{
  "timestamp": "2026-07-06T14:43:41Z",
  "event": "TechnologyBroker",
  "BrokerType": "human",
  "MarketID": 4367059459,
  "ItemsUnlocked": [
    {
      "Name": "Int_Hyperdrive_Overcharge_Size5_Class5",
      "Name_Localised": "$Int_Hyperdrive_Overcharge_Size2_Class3_Name;"
    }
  ],
  "Commodities": [
    {
      "Name": "thargoidtitandrivecomponent",
      "Name_Localised": "Titan Drive Component",
      "Count": 1
    }
  ],
  "Materials": [
    {
      "Name": "tellurium",
      "Count": 24,
      "Category": "Raw"
    },
    {
      "Name": "electrochemicalarrays",
      "Name_Localised": "Electrochemical Arrays",
      "Count": 12,
      "Category": "Manufactured"
    },
    "... (5 elementow lacznie)"
  ]
}
```

#### DataScanned

*9 wystapien*

```json
{
  "timestamp": "2026-06-21T10:57:24Z",
  "event": "DataScanned",
  "Type": "$Datascan_ListeningPost;",
  "Type_Localised": "Listening Post"
}
```

#### QuitACrew

*8 wystapien*

```json
{
  "timestamp": "2026-08-09T17:41:50Z",
  "event": "QuitACrew",
  "Captain": ""
}
```

#### SellSuit

*7 wystapien*

```json
{
  "timestamp": "2026-06-19T20:14:52Z",
  "event": "SellSuit",
  "SuitID": 1865175141849049,
  "SuitMods": [],
  "Name": "tacticalsuit_class1",
  "Name_Localised": "Dominator Suit",
  "Price": 90000
}
```

#### CancelDropship

*6 wystapien*

```json
{
  "timestamp": "2026-09-21T00:11:13Z",
  "event": "CancelDropship",
  "Refund": 0
}
```

#### VehicleSwitch

*6 wystapien*

```json
{
  "timestamp": "2026-02-02T06:46:02Z",
  "event": "VehicleSwitch",
  "To": "Mothership"
}
```

#### ShipyardRedeem

*5 wystapien*

```json
{
  "timestamp": "2026-08-03T12:56:55Z",
  "event": "ShipyardRedeem",
  "ShipType": "mediumtransport01",
  "ShipType_Localised": "Lynx Highliner",
  "BundleID": 129043860,
  "MarketID": 3708677888
}
```

#### ShipRedeemed

*5 wystapien*

```json
{
  "timestamp": "2026-08-03T12:56:55Z",
  "event": "ShipRedeemed",
  "ShipType": "mediumtransport01",
  "ShipType_Localised": "Lynx Highliner",
  "NewShipID": 28
}
```

#### SellWeapon

*5 wystapien*

```json
{
  "timestamp": "2026-05-26T16:31:39Z",
  "event": "SellWeapon",
  "Name": "wpn_s_pistol_kinetic_sauto",
  "Name_Localised": "Karma P-15",
  "Class": 2,
  "WeaponMods": [
    "weapon_suppression_unpressurised"
  ],
  "Price": 165000,
  "SuitModuleID": 1864543465742430
}
```

#### CommunityGoalDiscard

*4 wystapien*

```json
{
  "timestamp": "2026-08-11T18:38:16Z",
  "event": "CommunityGoalDiscard",
  "CGID": 855,
  "Name": "Carcosa Calls for Assistance to Distribute Celebratory Commodities",
  "System": "Carcosa"
}
```

#### JoinedSquadron

*4 wystapien*

```json
{
  "timestamp": "2026-07-28T11:57:12Z",
  "event": "JoinedSquadron",
  "SquadronID": 111649,
  "SquadronName": "REDA NAUGHTY HUNTERS"
}
```

#### Powerplay

*4 wystapien*

```json
{
  "timestamp": "2026-07-15T21:22:55Z",
  "event": "Powerplay",
  "Power": "Yuri Grom",
  "Rank": 0,
  "Merits": 7,
  "TimePledged": 35570
}
```

#### LoadoutRemoveModule

*4 wystapien*

```json
{
  "timestamp": "2026-07-09T20:08:08Z",
  "event": "LoadoutRemoveModule",
  "LoadoutName": "operations",
  "SuitID": 1865631930637434,
  "SuitName": "tacticalsuit_class5",
  "SuitName_Localised": "$TacticalSuit_Class1_Name;",
  "LoadoutID": 4293000009,
  "SlotName": "PrimaryWeapon1",
  "ModuleName": "wpn_m_assaultrifle_kinetic_fauto",
  "ModuleName_Localised": "Karma AR-50",
  "Class": 5,
  "SuitModuleID": 1869684929541417,
  "WeaponMods": [
    "weapon_clipsize",
    "weapon_backpackreloading",
    "... (4 elementow lacznie)"
  ]
}
```

#### ShipyardSell

*3 wystapien*

```json
{
  "timestamp": "2026-09-22T02:12:58Z",
  "event": "ShipyardSell",
  "ShipType": "sidewinder",
  "SellShipID": 62,
  "ShipPrice": 28080,
  "MarketID": 4308724739
}
```

#### ColonisationSystemClaimRelease

*3 wystapien*

```json
{
  "timestamp": "2026-08-22T08:31:25Z",
  "event": "ColonisationSystemClaimRelease",
  "StarSystem": "Bleia Eohn WW-J a63-2",
  "SystemAddress": 40525700551192
}
```

#### CarrierBuy

*3 wystapien*

```json
{
  "timestamp": "2026-08-01T10:54:31Z",
  "event": "CarrierBuy",
  "CarrierType": "FleetCarrier",
  "CarrierID": 3708677888,
  "BoughtAtMarket": 128667761,
  "Location": "Tir",
  "SystemAddress": 48996147307082,
  "Price": 5000000000,
  "Variant": "CarrierDockB",
  "Callsign": "N1M-L8V"
}
```

#### AppliedToSquadron

*2 wystapien*

```json
{
  "timestamp": "2026-07-28T09:49:59Z",
  "event": "AppliedToSquadron",
  "SquadronID": 111649,
  "SquadronName": "REDA NAUGHTY HUNTERS"
}
```

#### SquadronCreated

*2 wystapien*

```json
{
  "timestamp": "2026-07-26T22:04:24Z",
  "event": "SquadronCreated",
  "SquadronID": 117898,
  "SquadronName": "SYNOWIE SZATANA"
}
```

#### DockingTimeout

*2 wystapien*

```json
{
  "timestamp": "2026-05-18T18:38:50Z",
  "event": "DockingTimeout",
  "MarketID": 3715010304,
  "StationName": "PRNH",
  "StationType": "FleetCarrier"
}
```

#### EndCrewSession

*2 wystapien*

```json
{
  "timestamp": "2026-05-13T21:04:49Z",
  "event": "EndCrewSession",
  "OnCrime": false,
  "Telepresence": true
}
```

#### CarrierNameChange

*2 wystapien*

```json
{
  "timestamp": "2026-03-10T15:43:11Z",
  "event": "CarrierNameChange",
  "CarrierID": 3706381824,
  "": "FleetCarrier",
  "Name": "Maria Sklodowska",
  "Callsign": "W1V-NXM"
}
```

#### CockpitBreached

*2 wystapien*

```json
{
  "timestamp": "2025-12-14T12:44:27Z",
  "event": "CockpitBreached"
}
```

#### LeftSquadron

*1 wystapien*

```json
{
  "timestamp": "2026-07-26T21:55:25Z",
  "event": "LeftSquadron",
  "SquadronID": 111649,
  "SquadronName": "REDA NAUGHTY HUNTERS"
}
```

#### SelfDestruct

*1 wystapien*

```json
{
  "timestamp": "2026-07-21T23:55:53Z",
  "event": "SelfDestruct"
}
```

#### PowerplayLeave

*1 wystapien*

```json
{
  "timestamp": "2026-07-15T22:01:14Z",
  "event": "PowerplayLeave",
  "Power": "Yuri Grom"
}
```

#### PowerplayRank

*1 wystapien*

```json
{
  "timestamp": "2026-07-15T11:30:06Z",
  "event": "PowerplayRank",
  "Power": "Yuri Grom",
  "Rank": 0
}
```

#### PowerplayJoin

*1 wystapien*

```json
{
  "timestamp": "2026-07-15T11:30:06Z",
  "event": "PowerplayJoin",
  "Power": "Yuri Grom"
}
```

#### PowerplayMerits

*1 wystapien*

```json
{
  "timestamp": "2026-07-15T12:17:27Z",
  "event": "PowerplayMerits",
  "Power": "Yuri Grom",
  "MeritsGained": 7,
  "TotalMerits": 7
}
```

#### NewCommander

*1 wystapien*

```json
{
  "timestamp": "2026-05-14T13:33:39Z",
  "event": "NewCommander",
  "FID": "F13446110",
  "Name": "SJONAS DUNKAN IDAHO",
  "Package": "Default3"
}
```

#### CrewMemberJoins

*1 wystapien*

```json
{
  "timestamp": "2026-05-13T19:58:41Z",
  "event": "CrewMemberJoins",
  "Crew": "Cypher10110",
  "Telepresence": true
}
```

#### CrewMemberRoleChange

*1 wystapien*

```json
{
  "timestamp": "2026-05-13T19:58:41Z",
  "event": "CrewMemberRoleChange",
  "Crew": "Cypher10110",
  "Role": "Idle",
  "Telepresence": true
}
```

#### CrewMemberQuits

*1 wystapien*

```json
{
  "timestamp": "2026-05-13T20:27:28Z",
  "event": "CrewMemberQuits",
  "Crew": "Cypher10110",
  "Telepresence": true
}
```

#### ChangeCrewRole

*1 wystapien*

```json
{
  "timestamp": "2026-05-13T21:01:14Z",
  "event": "ChangeCrewRole",
  "Role": "OnFoot",
  "Telepresence": true
}
```

#### RepairDrone

*1 wystapien*

```json
{
  "timestamp": "2026-03-14T11:47:50Z",
  "event": "RepairDrone",
  "HullRepaired": 3.940849,
  "CockpitRepaired": 0.121391
}
```

#### CommunityGoalReward

*1 wystapien*

```json
{
  "timestamp": "2026-03-12T15:37:08Z",
  "event": "CommunityGoalReward",
  "CGID": 842,
  "Name": "Core Dynamics Combat Initiative",
  "System": "Duamta",
  "Reward": 130000000
}
```

#### CompleteConstruction

*1 wystapien*

```json
{
  "timestamp": "2026-01-28T22:30:26Z",
  "event": "CompleteConstruction"
}
```

#### RebootRepair

*1 wystapien*

```json
{
  "timestamp": "2026-01-25T13:00:35Z",
  "event": "RebootRepair",
  "Modules": [
    "Slot09_Size2"
  ]
}
```

#### BuyTradeData

*1 wystapien*

```json
{
  "timestamp": "2025-11-24T20:26:41Z",
  "event": "BuyTradeData",
  "System": "LHS 2936",
  "Cost": 100
}
```

---

## 2. Eventy znane w enumie, ktore nie przechodza parsowania

Lacznie **4996** linii zakonczonych `failed to parse`. Czesc z nich ma wpis w `event_e`,
`ScanOrganic` nie ma - ale wszystkie gina na tym samym etapie, przed dispatchem.
Kazda grupa ma inna przyczyne, warto je rozdzielic.

### Scanned (3657) i ScanOrganic (1292) - wspolna przyczyna

`generic_event_t` (include/elite_events.h:371) parsuje pole `ScanType` **dla kazdej linii journala**:

```cpp
struct generic_event_t
  {
  std::chrono::sys_seconds timestamp;
  std::string event;
  std::optional<scan_type_e> ScanType;
  };
```

a `scan_type_e` zna tylko trzy wartosci: `AutoScan`, `NavBeaconDetail`, `Detailed`.
W journalach wystepuje osiem:

| ScanType | wystapien | w `scan_type_e` |
|---|---:|---|
| `Detailed` | 17072 | tak |
| `AutoScan` | 15763 | tak |
| `Cargo` | 3648 | **nie** |
| `NavBeaconDetail` | 3205 | tak |
| `Sample` | 600 | **nie** |
| `Log` | 394 | **nie** |
| `Analyse` | 298 | **nie** |
| `Crime` | 9 | **nie** |

Skutek jest szerszy niz sam brak obslugi tych eventow: parse `generic_event_t` leci **przed**
rozpoznaniem typu eventu, wiec linia z nieznanym `ScanType` jest odrzucana w calosci i nigdy nie
trafia do `switch` w `generic_state_t::discovery`. `ScanOrganic` dodatkowo w ogole nie ma wpisu w `event_e`.

```json
{
  "timestamp": "2025-11-18T23:11:53Z",
  "event": "Scanned",
  "ScanType": "Cargo"
}
```

```json
{
  "timestamp": "2025-12-05T00:33:41Z",
  "event": "ScanOrganic",
  "ScanType": "Log",
  "Genus": "$Codex_Ent_Brancae_Name;",
  "Genus_Localised": "Brain Trees",
  "Species": "$Codex_Ent_SeedEFGH_Name;",
  "Species_Localised": "Lividum Brain Tree",
  "Variant": "$Codex_Ent_SeedEFGH_Name;",
  "Variant_Localised": "Lividum Brain Tree",
  "WasLogged": false,
  "SystemAddress": 182359951707,
  "Body": 20
}
```

### CarrierStats (46) - wartosc ujemna w polu bez znaku

`carrier_finance_t::AvailableBalance` jest `uint64_t`, a journal potrafi podac wartosc ujemna
(`"AvailableBalance":-9776434`). Poza tym struktura nie zna `TaxRate_rearm` ani `TaxRate_repair`,
a ma `ReservePercent`, ktorego w tym wariancie eventu nie ma.

```json
{
  "timestamp": "2026-03-12T11:31:16Z",
  "event": "CarrierStats",
  "CarrierID": 3706381824,
  "CarrierType": "FleetCarrier",
  "Callsign": "W1V-NXM",
  "Name": "Maria Sklodowska",
  "DockingAccess": "all",
  "AllowNotorious": true,
  "FuelLevel": 932,
  "JumpRangeCurr": 500.0,
  "JumpRangeMax": 500.0,
  "PendingDecommission": false,
  "SpaceUsage": {
    "TotalCapacity": 25000,
    "Crew": 1080,
    "Cargo": 16907,
    "CargoSpaceReserved": 0,
    "ShipPacks": 0,
    "ModulePacks": 0,
    "FreeSpace": 7013
  },
  "Finance": {
    "CarrierBalance": 990223566,
    "ReserveBalance": 1000000000,
    "AvailableBalance": -9776434,
    "TaxRate_rearm": 100,
    "TaxRate_refuel": 100,
    "TaxRate_repair": 100
  },
  "Crew": [
    {
      "CrewRole": "BlackMarket",
      "Activated": false
    },
    {
      "CrewRole": "Captain",
      "Activated": true,
      "Enabled": true,
      "CrewName": "Phoebe Reyes"
    },
    "... (14 elementow lacznie)"
  ],
  "ShipPacks": [],
  "ModulePacks": []
}
```

### FSDJump (1) - nie blad kodu

Jednorazowy uciety zapis w journalu - linia konczy sie w polowie JSON-a
(`"Government":"An`). Gra przerwala zapis, parser zachowal sie poprawnie. Nic do poprawy.

