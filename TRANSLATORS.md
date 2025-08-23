# Instructions for Translators

Thank you for your interest in translating Bazaar! 🏷️🗺️💜

Some basic rules:
* You must be fluent in the language you contribute
* You may not use llms to generate the strings (I could do that). If
  you do, I will ban you from the project
* If you are editing existing translation, make sure to check rules for that language
  in `TRANSLATORS-[language code].md` file.

## Basic Process

Fork the project (so you can open a pr later) and clone the repo. Then
make sure your current directory is the bazaar project root:

```
# Replace '...' with the URL of your Bazaar fork
# for which you have write permissions
git clone ...
cd bazaar
```
# Automatic Setup
Once you've done that, you can run `./translators.sh` and follow
instructions present on the screen. The script will show you what
`po/LINGUAS` currently looks like. If everything is correct, type `Y`
and press enter. After that the script will ask you to enter language
code, please enter it, and press enter. The script will now generate a
new `po` file or update an existing one such that any new translatable
strings will be available.

You are now ready to open your `po` file in your text editor or
translation editor (POEdit, GTranslator, Lokalize, etc.) and begin
translating. When you are done, commit your changes and submit a pull
request on github.

# Manual Setup

Once you've done that, setup the project with meson with the
`im_a_translator` flag set to `true`:

```sh
meson setup build -Dim_a_translator=true
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

Next, enter the build directory:

```sh
cd build
```

Run this command to generate the main `pot` (**P**ortable **O**bject
**T**emplate) file:

```sh
meson compile bazaar-pot
```

You might get a bunch of output complaining that the `blp` extension
is unknown. You can ignore this.

Finally, still inside the build directory, run the following command
to update and/or create the `po` (**P**ortable **O**bject) files:

```sh
meson compile bazaar-update-po
```

You are now ready to open your `po` file in your text editor or
translation editor (POEdit, GTr`anslator, Lokalize, etc.)and begin translating.
When you are done, commit your changes and submit a pull request on github.

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
