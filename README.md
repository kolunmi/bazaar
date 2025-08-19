# Bazaar

<div align="center">
<img src="data/icons/hicolor/scalable/apps/io.github.kolunmi.Bazaar.svg" width="128" height="128" />
</div>

A new app store for GNOME with a focus on discovering and installing
applications and add-ons from Flatpak remotes, particularly
[Flathub](https://flathub.org/). It emphasizes supporting the
developers who make the Linux desktop possible. Bazaar features a
"curated" tab that can be configured by distributors to allow for a
more locallized experience.

Bazaar is fast and highly multi-threaded, guaranteeing a smooth
experience in the user interface. You can queue as many downloads as 
you wish and run them while perusing Flathub's latest releases. 
This is due to the UI being completely decoupled from all backend operations.

It runs as a service, meaning state will be maintained even if you
close all windows, and implements the gnome-shell search provider dbus
interface. A krunner
[plugin](https://github.com/ublue-os/krunner-bazaar) is available for
use on the KDE Plasma desktop.

<div align="center">
<img height="512" alt="Image" src="https://github.com/user-attachments/assets/c63c8256-aae4-48a7-a4b0-68f60af3f980" />
</div>

Thanks to everyone in the GNOME development community for creating
such an awesome desktop environment!

If you would like to try this project on your local machine, clone it
on the cli and type these commands inside the project root:

```sh
meson setup build --prefix=/usr/local
ninja -C build
sudo ninja -C build install
bazaar window --auto-service
```

You will need the following dependencies installed, along with a C compiler, meson, and ninja:
| Dep Name                                                | `pkg-config` Name | Min Version            | Justification                                       |
|---------------------------------------------------------|-------------------|------------------------|-----------------------------------------------------|
| [gtk4](https://gitlab.gnome.org/GNOME/gtk/)             | `gtk4`            | enforced by libadwaita | GUI                                                 |
| [libadwaita](https://gitlab.gnome.org/GNOME/libadwaita) | `libadwaita-1`    | `1.7`                  | GNOME styling                                       |
| [libdex](https://gitlab.gnome.org/GNOME/libdex)         | `libdex-1`        | `0.11.1`               | Async helpers                                       |
| [flatpak](https://github.com/flatpak/flatpak)           | `flatpak`         | `1.9`                  | Flatpak installation management                     |
| [appstream](https://github.com/ximion/appstream)        | `appstream`       | `1.0`                  | Download application metadata                       |
| [xmlb](https://github.com/hughsie/libxmlb)              | `xmlb`            | `0.3.4`                | Handle binary xml appstream bundles/Parse plain xml |
| [glycin](https://gitlab.gnome.org/GNOME/glycin)         | `glycin-1`        | `1.0`                  | Retrieve and decode image uris                      |
| [glycin-gtk4](https://gitlab.gnome.org/GNOME/glycin)    | `glycin-gtk4-1`   | `1.0`                  | Convert glycin frames to `GdkTexture`s              |
| [libyaml](https://github.com/yaml/libyaml)              | `yaml-0.1`        | `0.2.5`                | Parse YAML configs                                  |
| [libsoup](https://gitlab.gnome.org/GNOME/libsoup)       | `libsoup-3.0`     | `3.6.0`                | HTTP operations                                     |
| [json-glib](https://gitlab.gnome.org/GNOME/json-glib)   | `json-glib-1.0`   | `1.10.0`               | Parse HTTP replies from Flathub                     |


## Supporting

If you would like to support me and the development of this
application (Thank you!), I have a ko-fi here!
https://ko-fi.com/kolunmi
