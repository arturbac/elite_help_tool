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
| `EHT_OVERLAY_STATS=0` | chowa własną diagnostykę warstwy w prawym górnym rogu |
| `EHT_OVERLAY_SCALE` | skala czcionki, domyślnie wysokość ekranu / 1080 |
| `EHT_OVERLAY_SIDE_WIDTH` | szerokość pasa przy krawędzi, domyślnie 20% szerokości, najwyżej 1600 px |
| `EHT_OVERLAY_SOCKET` | ścieżka gniazda, domyślnie `~/.local/share/elite_help_tool/overlay.sock` |

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
