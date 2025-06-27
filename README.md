# Bazaar

<div align="center">
<img src="data/icons/hicolor/scalable/apps/io.github.kolunmi.bazaar.svg" width="256" height="256" />
</div>

![Image](https://github.com/user-attachments/assets/57e235a2-d01d-47fc-8d56-ac9d4ecc8b51)

A new app store idea for GNOME. This is very much work-in-progress,
and nothing about the current state of the project is final; I intend
to add everything necessary to eclipse gnome-software in
functionality, at least as far as flatpak is concerned. Much of the
flatpak related code is referenced from gnome-software, which you can
find [here](https://gitlab.gnome.org/GNOME/gnome-software). Thanks to
everyone in the GNOME development community for creating such an
awesome desktop environment!

If you would like to try this project on your local machine install these dependencies and follow the build instructions:
## Build and Runtime Dependencies
Build Tools:

```
meson
ninja-build
pkg-config
cmake 
git 
```
Core GNOME/GTK Libraries:

```
libglib2.0-dev
libgtk-4-dev
libadwaita-1-dev
libsoup-3.0-dev
libjson-glib-dev libflatpak-dev
```
Special Dependencies:
```
libdex-1-dev (or libdex-dev depending on your distro)
libglycin-1-dev (or libglycin-gtk4-1-dev)
libxmlb-dev
libheif-dev
libjxl-dev 
rustc (for building glycin from source if needed)
```

For installation (just copy and paste this):
```sh
git clone https://github.com/kolunmi/bazaar
cd bazaar
meson setup build --prefix=/usr/local
ninja -C build
sudo ninja -C build install
bazaar window --auto-service
```

## Supporting

If you would like to support me and the development of this
application (Thank you!), I have a ko-fi here!
https://ko-fi.com/kolunmi

## Instructions for Distributors

If you are a user, this section should be irrelevant to you, but it
still may be interesting.

### Configuring the Browser

As of writing, the browser does not show anything unless you customize
it by detailing "sections" inside any number of YAML configuration
files at locations of your choosing. I will walk you through how to
create one of these YAML files, but first Bazaar needs to know where
to look for them.

Inside `meson_options.txt` you will see an option named
`hardcoded_content_config_path`. This compile time option optionally
specifies a filepath from which to load the "default" configuration at
runtime. This file is NOT embedded into the binary. In case you are
unfamiliar with meson options, simply add the following to your setup
command:

```sh
meson setup build -Dhardcoded_content_config_path=/path/to/config
```

The other way to specify configuration files is with the
`--extra-content-config` cli argument. It takes a single filepath
argument, and you can use this option multiple times.

By the way, every configuration is independently and continuously
monitored for filesystem events throughout the lifetime of the
process. So, in the following tutorial, you can simply save the
configuration whenever you want to see your changes; Bazaar will
automatically reload the relevant parts of the browser.

Once you are set up, copy and paste this snippet into the
configuration and save it:

```yaml
sections:
  - title: "Section #1"
    subtitle: "The first section"
    description: "These are some of my favorite apps!"
    rows: 3
    # use the file:// prefix for a local file
    banner: https://pixls.us/articles/processing-a-nightscape-in-siril/resultat_03_final.jpg
    # can be "fill", "contain", "cover", or "scale-down"
    banner-fit: cover
    appids:
      - com.usebottles.bottles
      - io.mgba.mGBA
      - net.pcsx2.PCSX2
      - org.blender.Blender
      - org.desmume.DeSmuME
      - org.duckstation.DuckStation
      - org.freecad.FreeCAD
      - org.gimp.GIMP
      - org.gnome.Builder
      - org.gnome.Loupe
      - org.inkscape.Inkscape
      - org.kde.krita
```

Your window should now look something like this:

![Image](https://github.com/user-attachments/assets/26cd8977-95d0-4e21-903f-e7cc7abbd776)

You can play around with this a bit and customize it to fit your
needs, or even add new sections (There is no limit!). Unfortunately,
it may still seem a little visually bland. That's where custom css
comes in!

Delete the contents of your configuration and paste this snippet
instead:

```yaml
sections:
  - title: "Section #1"
    subtitle: "The first section"
    description: "These are some of my favorite apps!"
    rows: 3
    # use the file:// prefix for a local file
    banner: https://pixls.us/articles/processing-a-nightscape-in-siril/resultat_03_final.jpg
    # can be "fill", "contain", "cover", or "scale-down"
    banner-fit: cover
    appids:
      - com.usebottles.bottles
      - io.mgba.mGBA
      - net.pcsx2.PCSX2
      - org.blender.Blender
      - org.desmume.DeSmuME
      - org.duckstation.DuckStation
      - org.freecad.FreeCAD
      - org.gimp.GIMP
      - org.gnome.Builder
      - org.gnome.Loupe
      - org.inkscape.Inkscape
      - org.kde.krita
    classes:
      - my-section

css: |
  .my-section {
     border: 5px solid var(--accent-fg-color);
     background: linear-gradient(#ff7788, #553377);
  }
  .my-section entry {
     background-color: alpha(var(--accent-bg-color), 0.05);
  }
  .my-section banner {
     opacity: 0.8;
  }
  .my-section title {
     border-bottom: 2px solid var(--accent-fg-color);
  }
```

Now Bazaar should look like this:

![Image](https://github.com/user-attachments/assets/2fb5c399-380a-48be-be6c-c5c035c2b4a0)

Feel free to modify this to your liking. You can even add animations,
etc. These are the current custom css names at your disposal:

* banner
* banner-text
* banners
* description
* entry
* entry-grid
* subtitle
* title

To learn more about how to use css in gtk, see the following
resources:

* https://docs.gtk.org/gtk4/css-overview.html
* https://docs.gtk.org/gtk4/css-properties.html

To be continued
