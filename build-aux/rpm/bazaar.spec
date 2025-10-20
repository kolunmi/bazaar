# renovate: datasource=git-refs depName=https://github.com/kolunmi/bazaar.git
%global commit 84c30ce0ebf3d3c57bd6b77df40e5778a01e15e8
%global shortcommit %(c=%{commit}; echo ${c:0:7})
%global appid io.github.kolunmi.Bazaar

Name:           bazaar
Version:        %{shortcommit}
Release:        1%{?dist}
Summary:        A new app store idea for GNOME. 

License:        GPL-3.0-or-later
URL:            https://github.com/kolunmi/bazaar
Source0:        %{url}/archive/%{commit}.tar.gz

BuildRequires:  meson
BuildRequires:  libadwaita-devel
BuildRequires:  libxmlb-devel
BuildRequires:  flatpak-devel
BuildRequires:  glycin-devel
BuildRequires:  glycin-gtk4-devel
BuildRequires:  libdex-devel
BuildRequires:  desktop-file-utils
BuildRequires:  systemd-rpm-macros
Requires:       glycin-libs
Requires:       libadwaita

%description
%summary

%prep
%autosetup -n bazaar-%{commit}

%build
%meson
%meson_build

%install
%meson_install

%post
%systemd_user_post %{appid}.service

%preun
%systemd_user_preun %{appid}.service

%postun
%systemd_user_postun_with_restart %{appid}.service

%files
%license COPYING
%doc README.md
%{_datadir}/applications/%{appid}.desktop
%{_bindir}/%{name}
%{_datadir}/dbus-1/services/%{appid}.service
%{_userunitdir}/%{appid}.service
%{_datadir}/glib-2.0/schemas/%{appid}.gschema.xml
%{_datadir}/icons/hicolor/scalable/apps/%{appid}.svg
%{_datadir}/icons/hicolor/symbolic/apps/%{appid}-symbolic.svg
%{_datadir}/metainfo/%{appid}.metainfo.xml

%changelog
* Sat May 17 2025 Tulip Blossom <tulilirockz@proton.me>
- Init package
