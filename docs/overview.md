# Bazaar Overview

These docs are intended for distributors of Bazaar to learn how to configure the
application.

## Features
Here is a short list of some of the things Bazaar can do, beyond the basics of
managing software through the GUI:

* Queue an arbitrary number of transactions (a catch-all term for installations,
  updates, and removals) during which you may close all windows or continue to
  append new transactions freely

* Display a "Curated" page which you may extensively customize with YAML, as
  will be discussed later. This is a way to highlight applications which you
  believe might be of interest.
  * Curated configuration files are constantly monitored for filesystem events,
    meaning you can update them and see the changes immediate reflected in the
    GUI
  * You can have any number of curated configuration files; they will
    concatenate in the GUI in the order they were provided

* Access data from [Flathub](https://flathub.org/), such as the latest or most
  popular applications, and allow users to sign in to Flathub to manage
  bookmarked applications

* Run in the background and respond to desktop search queries with application
  info, using the same search routine as in-application
  - GNOME will work out of the box, as Bazaar implements the
    `org.gnome.Shell.SearchProvider2` dbus interface
  - KDE Plasma will require a [krunner
    plugin](https://github.com/ublue-os/krunner-bazaar)

* Hide applications you do not want users to see with blocklists, which is
  useful for discouraging the use of certain packages which you deem to be
  broken.

* Manage an arbitrary amount of windows and keep them synchronized

* Communicate with, and invoke operations on, the main daemon through the
  command line

## Blocklists

Blocklists are a way to ensure that users will never interact with a certain
application inside Bazaar by searching or browsing, either on the Flathub page
or via search. Blocked applications which are already installed still appear in
the library or when there are updates available for them. Under no circumstance
does Bazaar touch the underlying flatpak configuration in order to block or
allow apps.

### Blocklist Types

There are two kinds of blocklists supported by Bazaar:

#### YAML Blocklists

For those who are learn more effectively by seeing an example:

```yaml
  blocklists:
    - priority: 0
      block-regex:
        # block all ids matching this regex unconditionally
        - com\.place\..*
    - priority: -1 # lower number = more priority
      conditions:
        - match-locale:
            # only apply this rule if we have the Arabic locale
            regex: ar
      allow:
        - com.place.App3
        - com.place.App5
      allow-regex:
        - com\.place\..*\.ar
    - priority: -1
      conditions:
        - match-locale:
            regex: en.*
          # invert the result of this condition; so all locales which don't match
          # the regex
          post-process: invert
        - match-envvar:
            var: PATH
            regex: .*/usr/local/bin.*
      block:
        - com.other.App1
      allow:
        - com.place.App1
        - com.place.App2
```

YAML blocklists files are comprised of a list of sub "blocklists," which to
Bazaar means a **list of appids to block or allow**. These are optionally
**applied based on a list of provided conditions** and **composed with other
blocklists in the list based on a priority**.

Each blocklist may contain these fields:

* `priority`: a number which tells Bazaar how to layer this blocklist with the
  others, or how "important" this blocklist is. **A lower number means more
  priority**.

* `conditions`: a list of conditions to apply. Two condition types exist at the
  moment: `match-locale` and `match-envvar`. TODO: describe these

* `block`: a list of appids to block. These are matched without regex.

* `block-regex`: a list of appids to block. These allow regex syntax for special matching.

* `allow`: a list of appids to allow. These are matched without regex.

* `allow-regex`: a list of appids to allow. These allow regex syntax for special matching.

#### Basic Newline Separated TXT Blocklists

This type of blocklist is what Bazaar originally supported. They still function,
but it is encouraged to use YAML blocklists if any sort of pattern matching or
conditional inclusion is required.

```
# comments are supported
com.jetbrains.CLion
com.valvesoftware.Steam

# empty lines are allowed

io.neovim.nvim
net.lutris.Lutris
```

Warning: **Incorrect Buzzer** TXT blocklists do not support regular expressions.
This matches the literal text provided

```
com\.place\..*
```

### Blocklist FAQ

#### No Worky

Check that the path the the blocklist exists and that Bazaar can access it.
Also, sometimes host files accessed from a flatpak container require a special
prefix.

#### I want to to block a list of applications all the time, and also another list only on desktop environment X:

Say you want to block Bazaar and Steam unconditionally and Kate and KWrite if
the desktop is Plasma. You can use an environment variable condition to check
the value of `XDG_CURRENT_DESKTOP`:

```yaml
blocklists:
  - block:
      - io.github.kolunmi.Bazaar
      - com.valvesoftware.Steam

  - block:
      - org.kde.kate
      - org.kde.kwrite
    conditions:
      - match-envvar:
          var: XDG_CURRENT_DESKTOP
          regex: KDE
```

Let's add another for GNOME:
```yaml
blocklists:
  - block:
      - io.github.kolunmi.Bazaar
      - com.valvesoftware.Steam

  - block:
      - org.kde.kate
      - org.kde.kwrite
    conditions:
      - match-envvar:
          var: XDG_CURRENT_DESKTOP
          regex: KDE

  - block:
      - org.gnome.eog
      - org.gnome.Extensions
    conditions:
      - match-envvar:
          var: XDG_CURRENT_DESKTOP
          regex: GNOME
```

#### What about blocking an application by default, but allowing it if a condition resolves to true?

Taking from the previous example, let's allow Steam on KDE and GNOME, but block
it on other desktops. We'll need to give the DE-specific blocklists more
priority:

```yaml
blocklists:
  - priority: 1
    block:
      - io.github.kolunmi.Bazaar
      - com.valvesoftware.Steam

  - block:
      - org.kde.kate
      - org.kde.kwrite
    allow:
      - com.valvesoftware.Steam
    conditions:
      - match-envvar:
          var: XDG_CURRENT_DESKTOP
          regex: KDE

  - block:
      - org.gnome.eog
      - org.gnome.Extensions
    allow:
      - com.valvesoftware.Steam
    conditions:
      - match-envvar:
          var: XDG_CURRENT_DESKTOP
          regex: GNOME
```

You could also make a new blocklist matching both `KDE` and `GNOME`:

```yaml
blocklists:
  - priority: 1
    block:
      - io.github.kolunmi.Bazaar
      - com.valvesoftware.Steam

  - block:
      - org.kde.kate
      - org.kde.kwrite
    conditions:
      - match-envvar:
          var: XDG_CURRENT_DESKTOP
          regex: KDE

  - block:
      - org.gnome.eog
      - org.gnome.Extensions
    conditions:
      - match-envvar:
          var: XDG_CURRENT_DESKTOP
          regex: GNOME

  - allow:
      - com.valvesoftware.Steam
    conditions:
      - match-envvar:
          var: XDG_CURRENT_DESKTOP
          regex: KDE|GNOME
```
