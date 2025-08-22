# Instructions for Translators

Thank you for your interest in translating Bazaar! 🏷️🗺️💜

Some basic rules:
* You must be fluent in the language you contribute
* You may not use llms to generate the strings (I could do that). If
  you do, I will ban you from the project
* If you are editing existing translation, make sure to check rules for that language
  in TRANSLATORS-[language code].md` file.
  
## Basic Process

Fork the project (so you can open a pr later) and clone the repo. Then
make sure your current directory is the bazaar project root:

```
# Replace '...' with the URL of your Bazaar fork
# for which you have write permissions
git clone ...
cd bazaar
```

Add your language identifier to `po/LINGUAS`. For example, if you are
adding a Spanish translation, insert `es` into the newline-separated
list, keeping it sorted alphabetically. So if the `po/LINGUAS` file
currently looks like this:

```
# Please keep this file sorted alphabetically.
ab
en_GB
ms
```

you will edit the file to look like this:

```
# Please keep this file sorted alphabetically.
ab
en_GB
es
ms
```
You can now open terminal in your Bazaar fork folder and then run:
```
./translators.sh
```
now follow instructions present on the screen.
The script will show you how `po/LINGUAS` currently looks like,
if everything is correct type Y and press enter.
Now the scrpt will ask you to enter language code, please enter it,
and press enter. The script will now generate a new .po file or
update an existing one, so that there are all translatable lines
avaible.

You are now ready to open your `po` file in your text editor and begin
translating. When you are done, commit your changes and submit a pull
request on github.

## Update existing translations

Generate a fresh `.pot` file again (if necessary) with the commands from above.

```
msgmerge --update --verbose po/de.po po/bazaar.pot
```


## Test your translations

Adjust for your [Language code](https://en.wikipedia.org/wiki/List_of_ISO_639_language_codes)!

```
msgfmt po/de.po -o bazaar.mo
sudo cp bazaar.mo /usr/share/locale/de/LC_MESSAGES/
```

Make sure to kill all the background processes from bazaar first

```
killall bazaar
```

```
LANGUAGE=de bazaar window --auto-service
```
