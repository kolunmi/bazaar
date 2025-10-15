# Bazaar

<div align="center">
<img src="data/icons/hicolor/scalable/apps/io.github.kolunmi.Bazaar.svg" width="128" height="128" />
</div>

UWAGA: Jeśli jesteś dystrybutorem lub osobą zajmującą się pakietami i chcesz się dowiedzieć
jak dostwosowywać Bazaar, możesz zajrzeć do [dokumentacji](/docs/overview.org).

Bazaar to nowy sklep z aplikacjami z naciskiem na odkrywanie i
instalowanie aplikacji i dodatków z repozytoriów Flatpak, głownie z
[Flathuba](https://flathub.org/). Podkreśla on potrzebę wspierania
twórców aplikacji na Linuxa. Bazaar posiada również stronę "polecane",
która może być konfigurowana przez dystrybutorów pozwalając na bardziej
lokalne doświadczenia.

Bazaar jest szybki i wysoce wielowątkowy, gwarantujący płynne
działanie interfejsu. Możesz dodać do kolejki tyle zadań
ile zechcesz, i wykonywać je podczas przeglądania Flathuba.
Jest to możliwe dzięki oddzieleniu interfejsu od zadań backendowych.

Działa jako serwis, oznacza to, że może kontrynuować wykonywanie
zadań nawet po zamknięciu wszystkich jego okien, oraz implementuje
interfejs dbus dostawcy wyszukiwania gnome-shell. Dla użytkowników
KDE Plasma jest dostępny [dodatek do KRunnera](https://github.com/ublue-os/krunner-bazaar).

<div align="center">
<img height="512" alt="Image" src="https://github.com/user-attachments/assets/c63c8256-aae4-48a7-a4b0-68f60af3f980" />
</div>

Dziękujemy wszystkim ze społeczności deweloperskiej GNOME za stworzenie
takiego doskonałego środowiska pulpitu.

Jeśli chcesz wytestować ten projekt na swoim komputerze, sklonuj ten projekt i
z poziomu terminala w folderze projektu uruchom następujące komendy:

```sh
meson setup build --prefix=/usr/local
ninja -C build
sudo ninja -C build install
bazaar
```

Musisz mieć zainstalowane następujące zależności, w tym kompilator C, meson oraz ninja:
| Nazwa zależności                                        | Nazwa `pkg-config` | Minimalna wersja           | Uzasadnienie                                                  |
|---------------------------------------------------------|--------------------|----------------------------|---------------------------------------------------------------|
| [gtk4](https://gitlab.gnome.org/GNOME/gtk/)             | `gtk4`             | narzucone przez libadwaita | Interfejs Użytkownika                                         |
| [libadwaita](https://gitlab.gnome.org/GNOME/libadwaita) | `libadwaita-1`     | `1.7`                      | wygląd GNOME                                                  |
| [libdex](https://gitlab.gnome.org/GNOME/libdex)         | `libdex-1`         | `0.11.1`                   | Pomoce Async                                                  |
| [flatpak](https://github.com/flatpak/flatpak)           | `flatpak`          | `1.9`                      | Zarządzanie instalowaniem Flatpaków                           |
| [appstream](https://github.com/ximion/appstream)        | `appstream`        | `1.0`                      | Pobieranie metadanych aplikacji                               |
| [xmlb](https://github.com/hughsie/libxmlb)              | `xmlb`             | `0.3.4`                    | Obsługa binarnych pakietów xml appstream/Analiza zwykłego xml |
| [glycin](https://gitlab.gnome.org/GNOME/glycin)         | `glycin-1`         | `1.0`                      | Otrzymywanie i dekodowanie URI obrazów                        |
| [glycin-gtk4](https://gitlab.gnome.org/GNOME/glycin)    | `glycin-gtk4-1`    | `1.0`                      | Konwertowanie ramek glycin na `GdkTextur`y                    |
| [libyaml](https://github.com/yaml/libyaml)              | `yaml-0.1`         | `0.2.5`                    | Analiza plików YAML                                           |
| [libsoup](https://gitlab.gnome.org/GNOME/libsoup)       | `libsoup-3.0`      | `3.6.0`                    | Operowanie HTTP                                               |
| [json-glib](https://gitlab.gnome.org/GNOME/json-glib)   | `json-glib-1.0`    | `1.10.0`                   | Analiza odpowiedzi HTTP od Flathuba                           |


## Wspieranie

Jeśli chcesz wesprzeć mnie i rozwój tej aplikacji (Dziękuję Ci!)
Możesz to zrobić na moim Ko-Fi: https://ko-fi.com/kolunmi
