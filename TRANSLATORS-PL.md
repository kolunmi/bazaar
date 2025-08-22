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

Teraz w otwórz okno terminala w podstawie katalogu projektu,
i uruchom:
```
./translators.sh
```

teraz postępuj z instrukcjami na ekranie.
Skrypt pokaże ci jak aktualnie wygląda `po/LINGUAS`, jeśli wszystko się zgadza
napisz Y i naciśnij enter. Teraz skrypt poprosi ciebie o wpisanie kodu języka,
na który chcesz przetłumaczyć Bazaar, wpisz go i naciśnij enter. Skrypt
stworzy lub zaktualizuje instiejący plik, tak aby miał wszystkie przetłumaczalne
linijki.

Teraz możesz otworzyć folder `po` i otworzyć `[kod-języka].po` w ulubionym programie do
tłumaczenia, może to być Lokalize, GTranslator (Translation Editor), itp.


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
