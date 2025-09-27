<h1 align="center">
<img src="data/icons/hicolor/scalable/apps/io.github.kolunmi.Bazaar.svg" width="128" height="128" />
<br/>
Bazaar
</h1>

<p align="center">Discover and install applications</p>

<div align="center">
<img height="512" alt="Image" src="https://github.com/user-attachments/assets/6e2a3f5b-1a92-47ce-89b4-61864a452fd5" />
</div>

<div align="center">
<img height="512" alt="Image" src="https://github.com/user-attachments/assets/0a149911-7edb-48c4-84e7-d4e64be80c0d" />
</div>

Bazaar is a new app store for GNOME with a focus on discovering and
installing applications and add-ons from Flatpak remotes, particularly
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

Thanks to [Jakub Steiner](http://jimmac.eu) for designing Bazaar's
icon.

The screenshot above showing the curated tab features
[Aurora](https://getaurora.dev/en)'s
[config](https://github.com/ublue-os/aurora/blob/9e66ef4f4624afa96fd6050f096c835ef0f81ad9/system_files/shared/usr/share/ublue-os/bazaar/config.yaml).

### Installing

Pre-built binaries are distributed via Flathub and GitHub actions:

<a href='https://flathub.org/apps/details/io.github.kolunmi.Bazaar'><img width='240' alt='Download on Flathub' src='https://flathub.org/assets/badges/flathub-badge-en.png'/></a>

[![Build Flatpak and Upload Artifact](https://github.com/kolunmi/bazaar/actions/workflows/build-flatpak.yml/badge.svg)](https://github.com/kolunmi/bazaar/actions/workflows/build-flatpak.yml)

### Supporting

If you would like to support me and the development of this
application (Thank you!), I have a ko-fi here! <https://ko-fi.com/kolunmi> 

[![Ko-Fi](https://img.shields.io/badge/Ko--fi-F16061?style=for-the-badge&logo=ko-fi&logoColor=white)](https://ko-fi.com/kolunmi)

Thanks to everyone in the GNOME development community for creating
such an awesome desktop environment!

### Contributing

> [!NOTE]
> If you are a distributor/packager who would like to learn how to
customize Bazaar, take a look at the [docs](/docs/overview.org).

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
| [glycin](https://gitlab.gnome.org/GNOME/glycin)         | `glycin-2`        | `2.0`                  | Retrieve and decode image uris                      |
| [glycin-gtk4](https://gitlab.gnome.org/GNOME/glycin)    | `glycin-gtk4-2`   | `2.0`                  | Convert glycin frames to `GdkTexture`s              |
| [libyaml](https://github.com/yaml/libyaml)              | `yaml-0.1`        | `0.2.5`                | Parse YAML configs                                  |
| [libsoup](https://gitlab.gnome.org/GNOME/libsoup)       | `libsoup-3.0`     | `3.6.0`                | HTTP operations                                     |
| [json-glib](https://gitlab.gnome.org/GNOME/json-glib)   | `json-glib-1.0`   | `1.10.0`               | Parse HTTP replies from Flathub                     |


#### Code of Conduct

This project adheres to the [GNOME Code of Conduct](https://conduct.gnome.org/). By participating through any means, including PRs, Issues or Discussions, you are expected to uphold this code.
