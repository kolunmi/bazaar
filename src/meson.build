math = cc.find_library('m', required: false)


libdex_opts = [
  'introspection=disabled',
]

libadwaita_dep       = dependency('libadwaita-1', version: '>= 1.7')
libdex_dep           = dependency('libdex-1', version: '>= 0.11.1', default_options: libdex_opts)
flatpak_dep          = dependency('flatpak', version: '>= 1.9')
appstream_dep        = dependency('appstream', version: '>= 1.0')
xmlb_dep             = dependency('xmlb', version: '>= 0.3.4')
yaml_dep             = dependency('yaml-0.1', version: '>= 0.2.5')
libsoup_dep          = dependency('libsoup-3.0', version: '>= 3.6.0')
json_glib_dep        = dependency('json-glib-1.0', version: '>= 1.10.0')
glycin_dep           = dependency('glycin-1', version: '>= 1.0')
glycin_gtk4_dep      = dependency('glycin-gtk4-1', version: '>= 1.0')


dl_worker_sources = [
  'bz-env.c',
  'bz-global-state.c',
  'dl-worker.c',
]

dl_worker_deps = [
  math,
  libdex_dep,
  libsoup_dep,
  json_glib_dep,
]

dl_worker_exe = executable(dl_worker_bin_name, dl_worker_sources,
  dependencies: dl_worker_deps,
       install: true,
)


marshalers = gnome.genmarshal('bz-marshalers',
  sources: 'bz-marshalers.list',
  prefix: 'bz_marshal',
  valist_marshallers: true,
)

gdbus_src = gnome.gdbus_codegen(
  'gs-shell-search-provider-generated',
  'shell-search-provider-dbus-interfaces.xml',
  interface_prefix : 'org.gnome.',
  namespace : 'Bz',
)

bz_sources = [
  'bz-addons-dialog.c',
  'bz-app-tile.c',
  'bz-application-map-factory.c',
  'bz-application.c',
  'bz-async-texture.c',
  'bz-backend.c',
  'bz-browse-widget.c',
  'bz-comet-overlay.c',
  'bz-comet.c',
  'bz-content-provider.c',
  'bz-content-section.c',
  'bz-data-graph.c',
  'bz-data-point.c',
  'bz-decorated-screenshot.c',
  'bz-download-worker.c',
  'bz-dynamic-list-view.c',
  'bz-entry-cache-manager.c',
  'bz-entry-group.c',
  'bz-entry.c',
  'bz-env.c',
  'bz-error.c',
  'bz-flathub-category.c',
  'bz-flathub-page.c',
  'bz-flathub-state.c',
  'bz-flatpak-entry.c',
  'bz-flatpak-instance.c',
  'bz-full-view.c',
  'bz-global-progress.c',
  'bz-global-state.c',
  'bz-gnome-shell-search-provider.c',
  'bz-inhibited-scrollable.c',
  'bz-installed-page.c',
  'bz-io.c',
  'bz-issue.c',
  'bz-lazy-async-texture-model.c',
  'bz-preferences-dialog.c',
  'bz-progress-bar.c',
  'bz-release.c',
  'bz-result.c',
  'bz-review.c',
  'bz-screenshot.c',
  'bz-search-engine.c',
  'bz-search-result.c',
  'bz-search-widget.c',
  'bz-section-view.c',
  'bz-serializable.c',
  'bz-share-dialog.c',
  'bz-state-info.c',
  'bz-stats-dialog.c',
  'bz-transaction-manager.c',
  'bz-transaction-view.c',
  'bz-transaction.c',
  'bz-update-dialog.c',
  'bz-url.c',
  'bz-window.c',
  'bz-yaml-parser.c',
  'main.c',
]

bz_deps = [
  # bazaar_ui_dep,
  math,
  gtk_dep,
  libadwaita_dep,
  flatpak_dep,
  appstream_dep,
  xmlb_dep,
  yaml_dep,
  libsoup_dep,
  json_glib_dep,
  libdex_dep,
  glycin_dep,
  glycin_gtk4_dep,
]

blueprints = custom_target('blueprints',
  input: files(
    'bz-addons-dialog.blp',
    'bz-app-tile.blp',
    'bz-browse-widget.blp',
    'bz-decorated-screenshot.blp',
    'bz-flathub-page.blp',
    'bz-full-view.blp',
    'bz-installed-page.blp',
    'bz-preferences-dialog.blp',
    'bz-progress-bar.blp',
    'bz-search-widget.blp',
    'bz-section-view.blp',
    'bz-share-dialog.blp',
    'bz-stats-dialog.blp',
    'bz-transaction-view.blp',
    'bz-update-dialog.blp',
    'bz-window.blp',
    'gtk/help-overlay.blp',
  ),
  output: '.',
  command: [
    find_program('blueprint-compiler', version: '>= 0.18.0'),
    'batch-compile',
    '@OUTPUT@',
    '@CURRENT_SOURCE_DIR@',
    '@INPUT@'
  ],
)

bz_sources += gnome.compile_resources('bz-resources',
  'bazaar.gresource.xml',
  c_name: 'bz',
  dependencies: blueprints
)

executable('bazaar', bz_sources, gdbus_src, marshalers,
           dependencies: bz_deps,
           install: true,
)
