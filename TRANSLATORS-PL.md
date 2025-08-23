# Instrukcje dla tłumaczy

Dziękuję za wasze zainteresowanie w tłumaczeniu Bazaar'u! 🏷️🗺️💜

Oto parę podstawowych zasad:
* Musisz być biegły w języku, na który będziesz tłumaczyć.
* Nie korzystaj z [SI](https://pl.wikipedia.org/wiki/Sztuczna_inteligencja), aby wygenerować tłumaczenie (Sam mógłbym to zrobić).
  Jeśli to zrobisz, zablokuję cię.
* Jeśli edytujesz istniejące tłumaczenie zapoznaj się z `TRANSLATORS-[kod języka].md`, aby zapoznać się z zasadami obecnymi dla tego języka.

Tłumaczenie na język polski:
* Zanim rozpoczniesz tłumaczenie zapoznaj się z http://fsc.com.pl/poradnik/
* Przydatne strony:
  - Słownik Diki - https://www.diki.pl/
  - Słownik Bab.la - https://bab.la/
  - Słownik Cambridge - https://dictionary.cambridge.org/
* Słowniki zapisane w podpunkcie "Przydatne strony" mają funkcje tłumaczenia całych wyrażen i zdań, lecz proszę z nich nie korzystać, bo to nie są dokładne tłumaczenia.
* Proszę również ogranicznyć korzystanie z serwisów takich jak Google Tłumacz, DeepL, itp.
  
## Procedury podstawowe

Utwórz fork projektu (tak abyś mógł później zrobić pull request) i skonuj repozytorium.
Następnie upewnij się, że jesteś w folderze odpowiadającym budową do podstawy repozytorium:

```
# Zmień '...' na adress URL twojego forku Bazaar'u,
# w którym masz uprawnienia do zapisywania
git clone ...
cd bazaar
```
Nie zamykaj okna terminala

# Konfiguracja Automatyczna

Dodaj kod języka docelowego do `po/LINGUAS`. Na przykład, jeśli dodajesz
hiszpańskie tłumaczenie, wstaw `es` do nowej linijki, upewniając się, że
lista jest w kolejności alfabetycznej. Zatem jeśli `po/LINGUAS` wygląda
następująco:

```
# Please keep this file sorted alphabetically.
ab
en_GB
ms
```

musisz zmienić to na:

```
# Please keep this file sorted alphabetically.
ab
en_GB
es
ms
```

Jak już to zrobisz, możesz uruchomić `./translators.sh` i podążać
za instrukcjami widocznymi na ekranie. Skrypt pokaże ci jak
plik `po/LINGUAS` aktualnie wygląda, jeśli wszystko się zgadza
naciśnij Y, a następnie enter. Następnie skrypt poprosi cię o wprowadzenie
kodu języka docelowego, wpisz go, a następnie naciśnij enter.
Następnie skrypt wygeneruje nowy plik .po lub zaktualizuje istniejący,
tak aby wszystkie przetłumaczalne linijki były dostępne.

Teraz jesteś gotowy, aby otworzyć swój plik `po` w wybranym edytorze tekstu
lub edytorze tłumaczeń (POEdit, GTranslator, Lokalize, itp.) i rozpocząć proces
tłumaczenia. Jak już skończysz, skommituj swoje zmiany i utwórz pull request na
githubie.

# Konfiguracja Ręczna

Jak już to zrobisz, skonfiguruj projekt za pomocą mesona z
flagą `im_a_translator` ustawioną na `true`:

```sh
meson setup build -Dim_a_translator=true
```

Dodaj kod języka docelowego do `po/LINGUAS`. Na przykład, jeśli dodajesz
hiszpańskie tłumaczenie, wstaw `es` do nowej linijki, upewniając się, że
lista jest w kolejności alfabetycznej. Zatem jeśli `po/LINGUAS` wygląda
następująco:

```
# Please keep this file sorted alphabetically.
ab
en_GB
ms
```

musisz zmienić to na:

```
# Please keep this file sorted alphabetically.
ab
en_GB
es
ms
```

Następnie, przejdź do katalogu `build`:

```sh
cd build
```

Uruchom tą komendę, aby wygenerować główny plik `pot`
(**P**ortable **O**bject **T**emplate):

```sh
meson compile bazaar-pot
```

Na wierszu poleceń może wyskoczyć mnóstwo błędów o tym, że rozszerzenie `blp`
jest nieznane. Możesz je zignorować.

Wreszcie, wciąż będąc w katalogu `build`, uruchom następującą komendę,
aby zaktualizować i/lub stworzyć pliki `po` (**P**ortable **O**bject):

```sh
meson compile bazaar-update-po
```

Teraz jesteś gotowy, aby otworzyć swój plik `po` w wybranym edytorze tekstu
lub edytorze tłumaczeń (POEdit, GTranslator, Lokalize, itp.) i rozpocząć proces
tłumaczenia. Jak już skończysz, skommituj swoje zmiany i utwórz pull request na
githubie.

## Testowanie swojego tłumaczenia

Dostosuj do swojego [kodu języka](https://en.wikipedia.org/wiki/List_of_ISO_639_language_codes)!

```
msgfmt po/de.po -o bazaar.mo
sudo cp bazaar.mo /usr/share/locale/de/LC_MESSAGES/
```

Upewnij się, że zatrzymasz wszystkie utuchomione instancje Bazaar'u.

```
killall bazaar
```

```
LANGUAGE=de bazaar window --auto-service
```
