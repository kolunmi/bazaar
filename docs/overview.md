# Bazaar Overview

These docs are intended for distributors of Bazaar to learn how to configure the
application.

## Features
Here is a short list of some of the things Bazaar can do, beyond the basics of
managing software through the GUI:

* Queue an arbitrary number of transactions (a catch-all term for installations,
  updates, and removals) during which you may close all windows or continue to
  append new transactions freely

* Display a "Curated" page which you may extensively customize with YAML.
* This is a way to highlight applications which you believe might be of
  interest.
  * Curated configuration files are constantly monitored for filesystem events,
    changes immediately reflected in the GUI
  * Any number of curated configuration files; they will concatenate in the GUI
    in the order they were provided

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

## CLI Usage

You can start the bazaar daemon like this:
```
bazaar [ARGS] [PACKAGE PATH/URI]
```

To avoid spawning an initial window, use:
```
bazaar --no-window [ARGS] [PACKAGE PATH/URI]
```

`[PACKAGE PATH/URI]` could be a `.flatpakref` file. flatpak+https and regular
https is supported.

## Comptime Configuration

The only compile time meson option you should concern yourself with for
production is `hardcoded_main_config_path`. This embeds a path to the main
configuration file into Bazaar. If this is not defined at compile time, Bazaar
will never attempt to read a main config. See the next section of this document
to see an example config file.

## Main Configuration

This is the primary YAML configuration file for bazaar, as designated by the
`hardcoded_main_config_path` meson option. Here, you will point bazaar to where
your other configs are located. You can also define hooks in this file. See the
"Hooks" section of this document for an overview of the hooks system and an
example of integration into the main config.

### Example

```yaml
yaml-blocklist-paths:
  - /path/to/yaml/blocklist.yaml
  - /path/to/another/yaml/blocklist.yaml
  # Flatpak path with host-etc permission
  - /run/host/etc/bazaar/blocklist.yaml
txt-blocklist-paths:
  - /path/to/txt/blocklist.txt
  - /path/to/another/txt/blocklist.txt
  # Flatpak path with host-etc permission
  - /run/host/etc/bazaar/blocklist.txt
curated-config-paths:
  - /path/to/yaml/file.yaml
  - /path/to/another/yaml/file.yaml
  # Flatpak path with host-etc permission
  - /run/host/etc/bazaar/curated.yaml
```

## Blocklists

Blocklists are a way to ensure that users will never interact with a certain
application inside Bazaar by searching or browsing, either on the Flathub page
or via search.

Blocked applications which are already installed still appear in
the library or when there are updates available for them and via the system's
flatpak configuration. Under no circumstance does Bazaar touch the underlying
flatpak configuration in order to block or allow apps. The `flatpak` command
line tool is unaffected.

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

Check that the path of the blocklist exists and that Bazaar can access it. This command is useful for debugging this:

```
flatpak run --command=bash io.github.kolunmi.Bazaar
```

The `/etc` of the host system accessed from a Flatpak requires the `host-etc` permission.

This means `/etc/bazaar/banner.png` turns into `/run/host/etc/bazaar/banner.png`.

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

## Curated Section

If Bazaar is provided a non-zero amount of curated configs, an extra tab will
appear on the window's header bar called "Curated". This section is intended for
distributors to curate applications for users with a customizable interface.
Curated configs are YAML files. They are constantly monitored by Bazaar for
filesystem events, so when the config changes, Bazaar will automatically reload
the content.

Right now, curated configs are essentially composed of a list of "sections"
which appear stacked on top of each other inside of a scrollable viewport in the
order they appear in the YAML. Each section has certain properties you can
customize, like a title, an image banner URI, and of course a list of appids.

Bazaar maps the appids you provide to the best matching "entry group" from the
table of applications it was able to pull from remote sources (Simply put, an
entry group in Bazaar is a collection of applications which share the same appid
but come from different sources or installations). The entry group has a
designated "ui entry" which was previously determined in the refresh process to
have the most useful content associated with it as it pertains to presenting
things like icons, descriptions, screenshots, etc to the user.

When the user selects the app in the section, they are brought to a "full view"
where they can see a bunch of information stored inside or referenced by the ui
entry and choose to invoke transactions on the entry group, like installation
or removal.

Additionally, curated configs allow you to define a css block from which you
can reference classes inside sections and change the way gtk renders the
content.

### Example

Here are practical examples:

- [Aurora](https://github.com/get-aurora-dev/common/tree/0d86028dd0d737d1d0eee08205c33fc91997f155/system_files/shared/etc/bazaar) - https://getaurora.dev
- [Bluefin](https://github.com/projectbluefin/common/tree/a868eba107b91c4eae60b6d1d6d2e2cdf05eb1c8/system_files/bluefin/etc/bazaar) - https://projectbluefin.io
- [Bazzite](https://github.com/ublue-os/bazzite/blob/4cb928b7268d0cae38592ff112e061f972caed63/system_files/desktop/shared/usr/share/ublue-os/bazaar) - https://bazzite.gg

Here is a basic curated config:
```yaml
# Some css names at your disposal:
# - banner
# - banner-text
# - banners
# - description
# - subtitle
# - title
# - app-tile
# - app-tile-title
# - app-tile-verified-check
# - app-tile-description
# - app-tile-installed-indicator
# - app-tile-installed-pill
css: |
  .main-section {
    margin: 15px;
    border-radius: 25px;
  }
  .main-section banner-text {
    margin: 15px;
    color: white;
  }
  .background-1 {
    background: linear-gradient(45deg, #170a49, #52136c);
  }
  .background-1 title {
    border-bottom: 5px solid white;
  }
  .background-1 app-tile > button {
    background-color: alpha(white, 0.1);
  }
  .background-1 app-tile > button:hover {
    background-color: alpha(var(--accent-bg-color), 0.5);
  }
  .background-2 {
    background: linear-gradient(75deg, #51263c, #7104a9);
  }
  .background-2 app-tile > button:focus {
    background-color: alpha(var(--accent-bg-color), 0.5);
  }
  .background-2 app-tile-verified-check {
    color: orange;
  }

rows:
  - sections:
    - expand-horizontally: true

      category:
        title: "My Favorite Apps"
        subtitle: "These are really good and you should download them!"

        # can be https as well
        # If you want this to work with the Flatpak then use this path
        # file:///run/host/etc/bazaar/banner-1.jxl
        banner: file:///home/kolunmi/banner-1.jxl

        # Dynamically switching between light/dark variants of banners
        light-banner: file:///home/kolunmi/banner-light.png
        dark-banner: file:///home/kolunmi/banner-dark.png

        # can be "fill", "contain", "cover", or "scale-down"
        # see https://docs.gtk.org/gtk4/enum.ContentFit.html
        banner-fit: contain

        # can be "fill", "start", "end", or "center"
        # see https://docs.gtk.org/gtk4/enum.Align.html
        # halign -> "horizontal alignment"
        banner-text-halign: start
        # valign -> "vertical alignment"
        banner-text-valign: center

        # size in pixels
        banner-height: 400

        # "The horizontal alignment of the label text inside its size
        # allocation."
        # see https://docs.gtk.org/gtk4/property.Label.xalign.html
        banner-text-label-xalign: 0.0

        # appid list
        appids:
          - com.usebottles.bottles
          - io.mgba.mGBA
          - net.pcsx2.PCSX2
          - org.blender.Blender
          - org.desmume.DeSmuME
          - org.duckstation.DuckStation
          - org.freecad.FreeCAD

        # Show an "Install All" button
        enable-bulk-install: true

      # reference the classes we defined earlier
      classes:
        - main-section
        - background-1

      # The `classes` key (above) is for styling which we want to apply
      # all the time. If you want a style class to only be active in
      # light or dark mode, use `light-classes` or `dark-classes`:
      light-classes:
        - light-section
      dark-classes:
        - dark-section


  - sections:
    - category:
        title: "Some more awesome apps!"
        subtitle: "These are also pretty cool"
        banner: file:///home/kolunmi/banner-2.png
        banner-fit: contain
        banner-text-halign: end
        banner-text-valign: center
        banner-text-label-xalign: 1.0
        appids:
          - org.gimp.GIMP
          - org.gnome.Builder
          - org.gnome.Loupe
          - org.inkscape.Inkscape
      classes:
        - main-section
        - background-2
```

### Integrate the curated section + blocklist with the official Flatpak for Administrators/Vendors

For more practical examples check out the configuration from [Bluefin](https://github.com/projectbluefin/common/tree/a868eba107b91c4eae60b6d1d6d2e2cdf05eb1c8/system_files/bluefin/etc/bazaar) and [Aurora](https://github.com/get-aurora-dev/common/tree/0d86028dd0d737d1d0eee08205c33fc91997f155/system_files/shared/etc/bazaar).

Bazaar by default looks for a config file in `/etc/bazaar` or `/run/host/etc/bazaar` inside the sandbox, this is [configured on build time](https://github.com/flathub/io.github.kolunmi.Bazaar/blob/709faccd8c4198c5fdabf20eb4a98db98a5aa1c6/io.github.kolunmi.Bazaar.yaml#L43-L46) This needs permission to `/etc` which can be granted with the `filesystem=host-etc` permission, the build on Flathub doesn't have this permission by default.

This is not super straightforward to setup currently as Flatpak doesn't support overriding permissions in `/etc` or `/usr` yet, so you have to resort to `systemd-tmpfiles` to create this permission override in `/var/lib/flatpak/overrides/io.github.kolunmi.Bazaar`.

Here is how they did it:

- [tmpfiles](https://github.com/get-aurora-dev/common/blob/0d86028dd0d737d1d0eee08205c33fc91997f155/system_files/shared/usr/lib/tmpfiles.d/bazaar-flatpak.conf)

- [actual permission override](https://github.com/get-aurora-dev/common/blob/0d86028dd0d737d1d0eee08205c33fc91997f155/system_files/shared/usr/share/ublue-os/flatpak-overrides/io.github.kolunmi.Bazaar), the filepath for this doesn't really matter, just a way for you to ship the symlink with tmpfiles

## Hooks

Hooks are an advanced feature of Bazaar. In essence, they allow you to
programmatically react to events and define dialogs with which you can query
user input. Currently, the only events you can subscribe to are the
"before-transaction" and "after-transaction" events:

* `before-transaction`: run the hook right before a transaction is scheduled to
  begin

* `after-transaction`: run the hook after a transaction successfully completes

Hooks are run like a signal emission. After an event occurs, hooks that are
found to be of the appropriate type are evaluated in an order of priority.
Higher priority hooks have the ability to stop the emission from propagating
further downwards. In the case of some events, like "before-transaction", a hook
can also hint to Bazaar some action to take, in this case whether the
transaction should be aborted.

A shell snippet which is defined by you is evaluated with `/bin/sh -c` multiple
times over the course of a hook's execution. An invocation of the shell snippet
is referred to as a "stage". Your shell snippet (which of course could just
invoke another script written in whatever language you prefer) will be provided
a number of environment variables which together will describe the current
stage. Your snippet must react accordingly by printing a response to stdout,
which Bazaar will read back.

This opens up a lot of possibilities for customization; here are a few examples:

* You would like a certain appid to be added to steam after the user installs
  it, so you register a hook on "after-transaction" to query the user's
  permission with a custom dialog. If they confirm, your script will go forward
  with the task of setting up a steam shortcut.

* You would like to prevent users from installing a certain appid, as some other
  method of installation, such as a system package, would provide a superior
  experience. A blocklist could achieve this, but you don't like the idea of
  hiding anything from the user. A hook subscribed to the "before-transaction"
  event could issue a warning and ask for extra confirmation. If the user
  decides to listen to the warning, you can signal to Bazaar that the
  transaction should be aborted.

Here is an overview of the environment variables the shell snippet will receive:

* `BAZAAR_HOOK_INITIATED_UNIX_STAMP`: the unix timestamp in seconds at which
  this hook was first invoked (the number of seconds that have elapsed since
  1970-01-01 00:00:00 UTC)

* `BAZAAR_HOOK_INITIATED_UNIX_STAMP_USEC`: the unix timestamp in microseconds at
  which this hook was first invoked (the number of microseconds that have
  elapsed since 1970-01-01 00:00:00 UTC)

* `BAZAAR_HOOK_STAGE_IDX`: the number of stages this hook has run so far

* `BAZAAR_HOOK_ID`: the value of the "id" mapping

* `BAZAAR_HOOK_TYPE`: the value of the "when" mapping

* `BAZAAR_HOOK_WAS_ABORTED`: "true" if a dialog aborted the hook

* `BAZAAR_HOOK_DIALOG_ID`: if applicable, the id of the current dialog

* `BAZAAR_HOOK_DIALOG_RESPONSE_ID`: if applicable, the user response given
  through the current dialog

* `BAZAAR_TS_APPID`: if applicable, the appid of the entry Bazaar is currently
  dealing with

* `BAZAAR_TS_TYPE`: if applicable, the type of transaction being run. Can be
  "install", "update", or "removal".

* `BAZAAR_HOOK_STAGE`: the stage at which the hook is running. This will
  indicate what the shell body is instructed to do at this time. The shell body
  must respond by outputting to stdout with a valid answer; the structure of a
  valid answer will depend on the stage, and if the structure is not valid the
  hook will be abandoned. The shell body might be run multiple times by Bazaar
  over the course of a hook with this variable set to differing values in order
  know how to orchestrate events in the UI, so the shell body must be able to
  branch depending on the value. The value may be any of the following:

  * `setup`: the hook is starting. Respond with "ok" to continue the
    execution of this hook, or "pass" to skip it and move on to the
    next registered hook

  * `setup-dialog` Bazaar is ready to ask the user a question with one of the
    dialogs you've defined inside the "dialogs" mapping. `BAZAAR_HOOK_DIALOG_ID`
    will tell you which one. Respond with "ok" to spawn the dialog, or "pass" to
    skip the dialog.

  * `teardown-dialog` Bazaar has received input from the user after asking them
    a question with one of the dialogs you've defined inside the "dialogs"
    mapping. `BAZAAR_HOOK_DIALOG_ID` will tell you which one.
    `BAZAAR_HOOK_DIALOG_RESPONSE_ID` will tell you the response the user chose.
    Respond with "ok" to continue, or "abort" to stop the execution of this
    hook.

  * `catch` One of your dialogs has aborted. This is your chance to handle the
    error. Respond with "recover" to continue, or "abort" to confirm that the
    execution of this hook should indeed skip to the teardown stage.

  * `action` Everything so far has gone according to plan, so it is time to take
    whatever external action this hook exists for. Bazaar requires no response
    at this time.

  * `teardown` The hook is cleaning up. Respond with "continue" to propagate the
    signal emission to lower priority hooks, or "stop" to indicate the emission
    should stop. Alternatively, if this hook type should hint to Bazaar an
    action to take (such as the "before-transaction" hook), respond with
    "confirm" to hint that the action should be taken, or "deny" to prevent the
    action from being taken. Both "confirm" and "deny" imply the effect of
    "stop", and "stop" implies "confirm".

### Example

Hooks are defined in the main yaml config as indicated by the
`hardcoded_main_config_path` comptime var. Here is a basic example demonstrating
how to define a hook:

`hardcoded_main_config_path`:
```yaml
hooks:
  - id: handle-jetbrains
    when: before-transaction
    dialogs:
      - id: jetbrains-warning
        title: >-
          Jetbrains IDEs are not supported in this format
        # If true, render inline markup commands in body; see
        # https://docs.gtk.org/Pango/pango_markup.html
        body-use-markup: true
        body: >-
          This is a <a href="https://www.jetbrains.com/">Jetbrains</a>
          application and is not officially supported on Flatpak. We
          recommend using the Toolbox app to manage Jetbrains IDEs.
        # Determines which option will be assumed if the user hits the
        # escape key or otherwise cancels the dialog
        default-response-id: cancel
        options:
          - id: cancel
            string: "Cancel"
          - id: goto-web
            string: "Download Jetbrains Toolbox"
            # can be "destructive" or "suggested" or omit for no
            # styling
            style: suggested
    shell: exec /absolute/path/to/bazaar-jetbrains-hook.bash
```

`/absolute/path/to/bazaar-jetbrains-hook.bash`:
```bash
  #!/usr/bin/env bash


  handle_setup_stage() {

      # only proceed if the user is installing something
      if [ "$BAZAAR_TS_TYPE" = install ]; then
          case "$BAZAAR_TS_APPID" in
              com\.jetbrains\.*)
                  # since the appid belongs to jetbrains, we continue
                  # with the hook
                  echo 'ok'
                  ;;
              *)
                  # otherwise, skip this hook
                  echo 'pass'
                  ;;
          esac
      else
          echo 'pass'
      fi

  }


  handle_setup_dialog_stage() {

      # we don't need to do anything here right now, just let Bazaar
      # know we should continue setting up the dialog
      echo 'ok'

  }


  handle_teardown_dialog_stage() {

      case "$BAZAAR_HOOK_DIALOG_RESPONSE_ID" in
          goto-web)
              # if the user pressed "Download Jetbrains Toolbox",
              # continue
              echo 'ok'
              ;;
          *)
              # otherwise, let's not do anything
              echo 'abort'
              ;;
      esac

  }


  handle_catch_stage() {

      # this only happens if the `teardown-dialog` stage echoed "abort",
      # we could echo "recover" at this point to still go to the
      # `action` stage, but we have no reason to do that right now
      echo 'abort'

  }


  handle_action_stage() {

      # this is where we do the thing! it is important to use `nohup`
      # here so bazaar doesn't hang
      nohup xdg-open 'https://www.jetbrains.com/toolbox-app/'

  }


  handle_teardown_stage() {

      # Let's always prevent the user from installing Jetbrains stuff
      echo 'deny'

  }


  # Branch based on the stage
  case "$BAZAAR_HOOK_STAGE" in
      setup) handle_setup_stage ;;
      setup-dialog) handle_setup_dialog_stage ;;
      teardown-dialog) handle_teardown_dialog_stage ;;
      catch) handle_catch_stage ;;
      action) handle_action_stage ;;
      teardown) handle_teardown_stage ;;
  esac


  # exit successfully
  exit 0
```

## Translations in YAML Configs

For any string scalar property in YAML configs parsed by bazaar, you can
optionally provide a map of language code ids -> translated strings. This is for
strings only, thus it doesn't apply to scalars which are explicitly parsed as
numbers, etc.

For instance, this is valid:

```yaml
# ...
      category:
        title: "My Favorite Apps"
        subtitle: "These are really good and you should download them!"
# ...
```

This is also valid:

```yaml
# ...
      category:
        title:
          en: "My Favorite Apps"
          es: "Mis Aplicaciones Favoritas"
          ko: "내가 가장 좋아하는 앱들"
        subtitle:
          en: "These are really good and you should download them!"
          es: "¡Son realmente buenos y deberías descargarlos!"
          ko: "이것들은 정말 좋으니 꼭 다운로드하세요!"
# ...
```

(I just used google translate for these, sorry if they are bad)
