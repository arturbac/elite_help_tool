# elite_help_tool
Elite Dangerous exploration planet visiting optimisation tool

This is realtime tool helping explore system and map valuable planets, optimise visiting route I wrote in few horus using C++23 for my own pleasure of explroation in Elite Dangerous.

![at_work](at_work.png)

## Eksploracja

Prawy dolny róg overlaya idzie w kolejności pracy w nowym systemie:

1. **przylot** — gwiazda przylotu odkryta wcześniej („discovered before - jump on”) albo
   `UNDISCOVERED`; ile ciał z honka już zeskanowano,
2. **do zmapowania** — ciała warte sond, najdroższe pierwsze, zielone gdy pierwsze odkrycie,
3. **życie** — ciała z sygnałami biologicznymi i przy każdym rodzaju gatunek, jakim
   najprawdopodobniej jest na tym świecie, z ceną; zielone gdy wart lądowania (`bio_worth`, 5M).

Gatunek zgadywany jest z własnej historii: każdy pobrany gatunek zapisuje się razem ze światem
(klasa planety, atmosfera, temperatura, grawitacja, gwiazda nad nim), a rodzaj na nowym świecie
porównywany jest z tym, co już znaleziono pod tą samą atmosferą. Na 374 gatunkach z archiwum,
zgadując każdy z pozostałych, pierwsza odpowiedź trafia w 302 przypadkach, pierwsze dwie w 349.
Im więcej próbek, tym węższe przypuszczenie.

Na powierzchni, w miejscu czytnika celu po lewej od środka, stoi próbka w toku: gatunek, cena,
1/3–3/3 i odległość od najbliższej wcześniejszej próbki wobec zasięgu kolonii, liczona na żywo z
`Status.json` — zielone „sample”, gdy już wolno. Niżej, co jeszcze rośnie na tym ciele.

## Codex

Przy każdej próbce warstwa robi zdjęcie środka ekranu (patrz `overlay/README.md`), a narzędzie
odkłada je do `codex/pictures/` obok siebie, opisane w `codex/pictures.json`. `codex/index.html`
pisany jest z bazy przy starcie i po każdej próbce: rodzina po rodzinie, gatunek po gatunku —
cena, liczba znalezisk, zakres temperatur i grawitacji, atmosfery, światy i gwiazdy, zdjęcia,
tabela systemów i ciał oraz czego z rodziny jeszcze nie znaleziono. Otwiera go przycisk *Codex*.
Zdjęcia żyją w katalogu, nie w bazie: bazę da się odtworzyć z journali, zdjęcia nie.

Ustawienia w sekcji `exploration` pliku `eht_settings.json`: `bio_worth`, `bio_bodies`,
`candidates`, `capture`, `capture_size`, `codex_dir`, `jpeg_quality`.

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
