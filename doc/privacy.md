# Privacy and your own galaxy

EHT is cut off from the public databases **in both directions**, on purpose.

**Nothing comes in.** EHT downloads nothing: no EDSM, no Spansh, no Inara, no species lists or
system dumps. Everything it knows comes from your own journals and your own Cargo, Market and
Status files. The galaxy in its databases is the galaxy *you* have flown, scanned and traded in.
That is what makes the knowledge yours: the species guessed on a new world come only from what you
have sampled yourself, so the prediction learns from your own work and gets better the more you
explore, instead of handing you someone else's answers. The only other source it will read is the
`galaxy.sqlite` of another of your own accounts, if you point `exploration.shared_galaxies` at it.

**Little goes out, and only what you allow.** The one connection EHT ever makes is to
[EDDN](https://github.com/EDCD/EDDN), the players' shared data network, and only when you turn it
on:

- **Off by default.** `eddn.enabled` is `false`, and `eddn.test` sends to EDDN's test schemas, which
  reach no one, until you set it to `false`.
- **Per commander.** Only the FIDs listed in `eddn.exploration_commanders` and
  `eddn.bartender_commanders` send anything, each only its own kind of data.
- **Exploration only of empty, undiscovered systems.** Scan events (FSDJump, Scan, FSS and SAA
  signals, barycentres, codex entries) are sent only for systems where the arrival star was
  undiscovered and nobody lives. A populated system is never sent, whoever found it. This keeps the
  systems where you do BGS work out of public view.
- **Only after you sell.** Exploration messages wait in `eddn_held.jsonl` until you sell that
  system's cartographic data. Until then nobody else learns what you found.
- **Fleet carrier bar stock** (`fcmaterials_journal/1`) is the only other thing sent, and only for
  the commanders listed for it.
- **No markets, missions, BGS or cargo** are ever sent, and where you are shows only through the
  exploration messages above, after the sale. Localised text is stripped as EDDN asks. The message header carries your commander name as `uploaderID`, as with every EDDN
  sender.
