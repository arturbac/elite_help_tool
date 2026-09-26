# elite_help_tool
Elite Dangerous exploration planet visiting optimisation tool

This is realtime tool helping explore system and map valuable planets, optimise visiting route I wrote in few horus using C++23 for my own pleasure of explroation in Elite Dangerous.

![at_work](at_work.png)

## Trzy bazy

Dane rozkładają się na trzy pliki leżące obok siebie, bo różnią się pochodzeniem i właścicielem:

| plik | zawiera | odtwarzalny z journali | wspólny dla kont |
|---|---|---|---|
| `ehtdb.sqlite` | misje, zdobycze, reputacja, postęp skanowania | tak | **nie** — należy do jednej postaci |
| `galaxy.sqlite` | systemy, ciała, stacje, frakcje, wpływy, konflikty | tak | tak |
| `live.sqlite` | rynki, ceny, półka bartendera | **nie** | tak |

Rozdział `ehtdb` od `galaxy` ma jeden konkretny powód: świat jest ten sam dla każdego commandera,
ale „co zeskanowałem i zmapowałem" już nie. Pokazanie jednej postaci, że zmapowała planetę, którą
naprawdę zmapowała druga, prowadzi wprost do błędnej decyzji przy planowaniu lotu. Dlatego
`system_progress`, `body_progress`, `genus_progress` i `faction_reputation` zostają w bazie
osobistej i kluczują się **naturalnie** — adresem systemu, numerem ciała, nazwą frakcji — a nie
`oid`-ami, które zmieniają się przy każdej przebudowie `galaxy.sqlite`.

Dzięki temu dwa konta mogą wskazywać symlinkiem ten sam `galaxy.sqlite` i `live.sqlite`, dzieląc
wiedzę o galaktyce i cenach, a zachowując własne misje, reputację i postęp eksploracji.

`journal_tailer` kasuje i odtwarza `ehtdb.sqlite` oraz `galaxy.sqlite`; `live.sqlite` zostawia
nietknięty, bo jego zawartości nie da się odtworzyć z journali.
