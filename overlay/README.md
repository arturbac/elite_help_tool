# Overlay w oknie gry

Warstwa Vulkana rysująca informacje z `elite_help_tool` wprost w klatce Elite Dangerous.

## Dlaczego warstwa, a nie okno

Sesja Wayland nie pozwala postawić obcego okna nad grą, a w pełnym ekranie nie działa to nigdzie.
Warstwa Vulkana rysuje do obrazu łańcucha wymiany tuż przed prezentacją, więc trafia do obrazu gry
niezależnie od trybu okna. Elite idzie DX11 → Proton → DXVK → natywny Vulkan, więc warstwa go łapie.

## Z czego się składa

| plik | rola |
|---|---|
| `libeht_overlay.so` | warstwa Vulkana, loader wkłada ją **do procesu gry** |
| `eht-overlay-run` | opakowanie do `%command%` w Steam: ustawia zmienne i uruchamia grę |
| `eht-overlay-feed` | źródło testowe, wysyła ramki bez udziału głównej aplikacji |
| `eht-overlay-headless-check` | render bez ekranu do pliku PPM, do sprawdzania warstwy |

Serwer siedzi w `elite_help_tool` — to on decyduje, co ma się pojawić. Warstwa dostaje gotowe linie
tekstu i nie wie nic o bazie; dzięki temu w procesie gry nie ma ani SQLite, ani żadnego stanu.

## Instalacja

```
ninja overlay-install
```

Kopiuje warstwę do `~/.local/lib`, opakowanie do `~/.local/bin`, manifest do
`~/.local/share/vulkan/implicit_layer.d`. Musi to być katalog domowy — kontener Steam Linux Runtime
nie widzi katalogu budowania.

W opcjach uruchamiania Steam dla Elite Dangerous najprościej bez opakowania, bo manifest leży
w standardowej ścieżce, której loader i tak szuka:

```
ENABLE_EHT_OVERLAY=1 %command%
```

Z opakowaniem **konieczna jest pełna ścieżka**. Steam uruchamia opcje przez `/bin/sh`, które nie ma
`~/.local/bin` w `PATH`, a sama nazwa kończy się `command not found` i gra nie startuje wcale:

```
/home/artur/.local/bin/eht-overlay-run %command%
```

Elite startuje przez własny launcher, a zmienne środowiskowe dziedziczą procesy potomne, więc gra
dostaje je tak samo jak launcher. Launcher nie ma łańcucha wymiany, więc warstwa nic w nim nie robi.

## Zmienne środowiskowe

| zmienna | działanie |
|---|---|
| `ENABLE_EHT_OVERLAY=1` | włącza warstwę |
| `DISABLE_EHT_OVERLAY=1` | wyłącza ją mimo zainstalowanego manifestu — wyłącznik awaryjny |
| `EHT_OVERLAY_DEBUG=1` | log warstwy na stderr procesu gry |
| `EHT_OVERLAY_SOCKET` | ścieżka gniazda, domyślnie `~/.local/share/elite_help_tool/overlay.sock` |
| `EHT_OVERLAY_CAPTURE=1` | pozwala kopiować obraz gry — bez tego nie ma zdjęć do codexu ani zrzutów ekranu klawiszem |

Tylko te — potrzebne, zanim warstwa połączy się z narzędziem. Cały układ (skala tekstu, szerokość
pasów i środkowego ekranu, położenie czytników HUD, marginesy, przezroczystość, własna diagnostyka)
przychodzi od `elite_help_tool` w każdej ramce, z sekcji `overlay.layout` pliku `eht_settings.json`
w katalogu, z którego uruchomione jest narzędzie. Zapisana zmiana pojawia się w grze od razu; zmiana
skali tekstu przebudowuje czcionki warstwy bez restartu gry. Dawne `EHT_OVERLAY_SCALE`,
`_SIDE_WIDTH`, `_CENTRE_WIDTH`, `_HUD_GAP`, `_HUD_BOTTOM`, `_SMALL_TEXT` i `_STATS` nie są już czytane.

## Dwa konta gry naraz

Nic nie stoi na przeszkodzie, żeby obok siebie chodziły dwa komplety narzędzie + gra — na przykład
konto ze Steama i drugie z osobnego launchera. Warunek jest jeden: **każda para musi mieć własne
gniazdo**, bo serwer przy starcie kasuje plik gniazda, który zastał (żeby nie blokował go plik po
ubitym procesie). Dwie instancje na domyślnej ścieżce odbiorą sobie nawzajem połączenie.

```
# konto główne - bez zmian, wartości domyślne
elite_help_tool

# konto drugie - narzędzie i gra dostają tę samą, własną ścieżkę
EHT_OVERLAY_SOCKET=~/.local/share/elite_help_tool/overlay-alt.sock elite_help_tool
EHT_OVERLAY_SOCKET=~/.local/share/elite_help_tool/overlay-alt.sock ENABLE_EHT_OVERLAY=1 <launcher>
```

Ścieżka gniazda nie może przekroczyć 107 znaków — tyle mieści `sockaddr_un`. Dłuższa kończy się
brakiem nasłuchu, co widać w logu narzędzia razem z jej długością.

Każde konto potrzebuje też własnego katalogu roboczego narzędzia, bo `journal-dir` i `ehtdb.sqlite`
są względne do katalogu bieżącego, a misje i zdobycze należą do konkretnej postaci. Plik
`live.sqlite` przeciwnie — trzyma wyłącznie fakty o galaktyce (rynki, ceny, półki bartendera),
więc warto go podlinkować, żeby oba konta korzystały ze wspólnej wiedzy o cenach.

Pas boczny ma znaczenie na panoramicznych ekranach: przy 8000 px środek należy do gry, a overlay
mieści się w ~1600 px z każdej strony. Dłuższy tekst zawija się w pasie zamiast wjeżdżać na środek.

## Sprawdzenie widoczności w kontenerze Steam

Gra biegnie w kontenerze, który nie widzi katalogu budowania. Że loader w środku znajduje warstwę,
sprawdza się bez uruchamiania gry:

```
ENABLE_EHT_OVERLAY=1 ~/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4/run --   vulkaninfo --summary | grep EHT
```

Wiersz `VK_LAYER_EHT_overlay` na liście warstw oznacza, że ścieżka pod `$HOME` działa, a biblioteka
jest zgodna z biblioteką standardową C w kontenerze.

## Sprawdzenie bez gry

```
eht-overlay-feed &
ENABLE_EHT_OVERLAY=1 eht-overlay-headless-check /tmp/check.ppm 8000 1440
```

`VK_EXT_headless_surface` daje łańcuch wymiany, którego nikt nie ogląda — warstwa rysuje tak samo
jak w prawdziwym oknie, a obraz wraca do pliku. Nie potrzeba pulpitu ani gry.

## Zmierzone

Na RX 7900 XTX, 3000 klatek, RADV:

| pomiar | wynik |
|---|---|
| koszt na klatkę | 0,018 ms (0,040 → 0,058) — ok. 0,1% budżetu przy 60 fps |
| utworzenie zasobów warstwy | 0,2 ms |
| zwolnienie zasobów | 0,2 ms |
| 2000 odtworzeń łańcucha wymiany | bez awarii, przyrost pamięci 0,3 MB |

Ostatni wiersz odpowiada alt-tabowaniu i zmianom rozdzielczości w grze.

```
ENABLE_EHT_OVERLAY=1 eht-overlay-headless-check /tmp/x.ppm 1920 1080 10 200
```

## Zdjęcie środka ekranu

Przy każdej próbce organizmu narzędzie prosi warstwę o zdjęcie — commander patrzy wtedy prosto na
roślinę. Prośba jedzie w ramce (`frame_t.capture`: numer, ścieżka, bok kwadratu jako ułamek
wysokości ekranu) i powtarza się w każdej kolejnej ramce, więc prosi **zmiana numeru**, nie sama
obecność. Pierwszy numer, który świeżo uruchomiona warstwa zobaczy, uznaje za załatwiony — to
ostatnia ramka narzędzia podana nowemu klientowi, a chwila, dla której była, dawno minęła.

Warstwa kopiuje kwadrat ze środka obrazu gry **zanim** narysuje na nim overlay, odbiera piksele
dopiero po ogrodzeniu tej klatki i zapisuje PPM w osobnym wątku, pod docelową nazwą dopiero gdy
plik jest kompletny. Pliki trafiają obok gniazda (`~/.local/share/elite_help_tool/captures/`),
jedynego miejsca widocznego po obu stronach kontenera; narzędzie zamienia je na JPG w swoim
katalogu `codex/`.

Łańcuch wymiany dostaje dodatkowo `TRANSFER_SRC`; sterownik, który tego nie przyjmie, zostawia
overlay bez zdjęć, nie bez overlaya. Zdjęcie, które raz się nie uda, nie jest już próbowane.
Starsza warstwa pomija nowe pole i po prostu zdjęć nie robi.

## Zrzut ekranu klawiszem

W grze **F11** robi zrzut całego ekranu **razem z overlayem** — tak, jak widzi go gracz. Klawisz
i format ustawia sekcja `screenshots` w `eht_settings.json`:

| klucz | domyślnie | znaczenie |
|---|---|---|
| `key` | `F11` | nazwa klawisza X (jak w `xev`): `F1`–`F35`, `Print`, `Pause`, `Scroll_Lock`, `KP_Multiply`…, litera, cyfra albo keysym `0xffc8`; pusty wyłącza |
| `dir` | `screenshots` | katalog na zrzuty, względny do katalogu narzędzia |
| `format` | `png` | `png` zostawia ostry tekst overlaya, `jpg` jest wielokrotnie mniejszy |
| `jpeg_quality` | `92` | jakość dla `jpg` |

Klawisz słyszy sama warstwa, nie narzędzie: osobny wątek w procesie gry pyta serwer X (przez
`libxcb`, ładowane `dlopen` z procesu gry) o stan klawiatury co 30 ms. Pod XWayland serwer X zna
klawisze tylko wtedy, gdy fokus ma jedno z jego okien, a przy dwóch grach naraz liczy się tylko ta,
której okno jest aktywne (`_NET_ACTIVE_WINDOW` → `_NET_WM_PID` → ten sam proces albo ten sam
`WINEPREFIX`). Gra dostaje klawisz tak samo jak bez overlaya — warstwa tylko patrzy.

Kopia całego obrazu jest robiona **po** narysowaniu overlaya, trafia do spoolu jako
`screenshot_<ms>.ppm`, a narzędzie w osobnym wątku zapisuje ją jako `ED <data czas>.png` i kasuje PPM.
Zrzuty zrobione, gdy narzędzie nie działało, zapisze przy najbliższym starcie. Wymaga
`EHT_OVERLAY_CAPTURE=1`, jak zdjęcia do codexu.

Działa tylko z grą w oknie X11 (XWayland) — domyślnie tak uruchamia ją Proton. Z
`PROTON_ENABLE_WAYLAND=1` warstwa klawisza nie usłyszy.

## Czego overlay nie może zrobić

Nic z tego nie ma prawa położyć gry:

- nieudane utworzenie zasobów albo inicjalizacja ImGui oznaczają jednorazowe odpuszczenie
  danego łańcucha wymiany; prezentacja idzie dalej nietknięta,
- łańcuch wymiany, który nie przyjmuje dołożonej flagi użycia, tworzony jest po staremu, bez overlaya,
- czekanie na ogrodzenie ma limit sekundy, po którym warstwa wyłącza się sama,
- nieudane założenie ImGui trafia do logu zamiast przerywać proces,
- wątek klienta i rejestr są celowo nigdy nie zwalniane, żeby wygaszanie procesu gry nie
  mogło się o nie zablokować.

Kolejność uruchamiania jest dowolna: klient próbuje się łączyć w kółko, serwer przyjmuje kogo
popadnie i obu nie przeszkadza nieobecność ani restart drugiej strony. Nowo podłączona gra dostaje
ostatni obraz od razu, bez czekania na najbliższą zmianę.
