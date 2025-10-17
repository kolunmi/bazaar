<h1 align="center">
<img src="data/icons/hicolor/scalable/apps/io.github.kolunmi.Bazaar.svg" width="128" height="128" />
<br/>
Bazaar
</h1>

<p align="center">Discover and install applications</p>

<div align="center">
    <picture>
        <source srcset="https://github.com/user-attachments/assets/cc545658-31fc-4cc5-b512-a4c17a2af454" media="(prefers-color-scheme: dark)">
        <img height="512" alt="Screenshot showing Bazaar's Flathub page" src="https://github.com/user-attachments/assets/b712f5de-ea5f-4f06-b834-d41b9265a192">
    </picture>
</div>

Bazaar is a new app store for GNOME with a focus on discovering and installing
applications and add-ons from Flatpak remotes, particularly
[Flathub](https://flathub.org/). It emphasizes supporting the developers who
make the Linux desktop possible. Bazaar features a "curated" tab that can be
configured by distributors to allow for a more localized experience.

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
bazaar
```

You will need the following dependencies installed, along with a C compiler, meson, and ninja:
| Dep Name                                                | `pkg-config` Name | Min Version            | Justification                                       |
|---------------------------------------------------------|-------------------|------------------------|-----------------------------------------------------|
| [gtk4](https://gitlab.gnome.org/GNOME/gtk/)             | `gtk4`            | enforced by libadwaita | GUI                                                 |
| [libadwaita](https://gitlab.gnome.org/GNOME/libadwaita) | `libadwaita-1`    | `1.8`                  | GNOME styling                                       |
| [libdex](https://gitlab.gnome.org/GNOME/libdex)         | `libdex-1`        | `1.0`                  | Async helpers                                       |
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

### What people are saying

- [Why Bazaar Is the Best Flatpak App Store You’re Not Using](https://fossforce.com/2025/10/why-bazaar-is-the-best-flatpak-app-store-youre-not-using/)
- [Bazaar is a game changer](https://gardinerbryant.com/linux-software-management-is-about-to-change-with-bazaar/)
- [Bazaar is a Slick New Desktop Flathub Frontend](https://www.omgubuntu.co.uk/2025/08/bazaar-new-flatpak-app-store-gnome-linux)
- [Bazaar Is the Flatpak Store GNOME Always Needed](https://linuxiac.com/bazaar-is-the-flatpak-store-gnome-always-needed/)
