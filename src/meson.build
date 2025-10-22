math = cc.find_library('m', required: false)


libdex_opts = [
  'introspection=disabled',
]

libadwaita_dep       = dependency('libadwaita-1', version: '>= 1.7')
libdex_dep           = dependency('libdex-1', version: '>= 1.0.0', default_options: libdex_opts)
flatpak_dep          = dependency('flatpak', version: '>= 1.9')
appstream_dep        = dependency('appstream', version: '>= 1.0')
xmlb_dep             = dependency('xmlb', version: '>= 0.3.4')
yaml_dep             = dependency('yaml-0.1', version: '>= 0.2.5')
libsoup_dep          = dependency('libsoup-3.0', version: '>= 3.6.0')
json_glib_dep        = dependency('json-glib-1.0', version: '>= 1.10.0')
glycin_dep           = dependency('glycin-2', version: '>= 2.0')
glycin_gtk4_dep      = dependency('glycin-gtk4-2', version: '>= 2.0')


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
  'bz-app-size-dialog.c',
  'bz-app-tile.c',
  'bz-application-map-factory.c',
  'bz-application.c',
  'bz-apps-page.c',
  'bz-appstream-description-render.c',
  'bz-async-texture.c',
  'bz-backend-notification.c',
  'bz-backend-transaction-op-payload.c',
  'bz-backend-transaction-op-progress-payload.c',
  'bz-backend.c',
  'bz-browse-widget.c',
  'bz-category-tile.c',
  'bz-comet-overlay.c',
  'bz-comet.c',
  'bz-content-provider.c',
  'bz-content-section.c',
  'bz-country-data-point.c',
  'bz-country.c',
  'bz-curated-app-tile.c',
  'bz-data-graph.c',
  'bz-data-point.c',
  'bz-decorated-screenshot.c',
  'bz-detailed-app-tile.c',
  'bz-download-worker.c',
  'bz-dynamic-list-view.c',
  'bz-entry-cache-manager.c',
  'bz-entry-group.c',
  'bz-entry-inspector.c',
  'bz-entry.c',
  'bz-env.c',
  'bz-error.c',
  'bz-featured-carousel.c',
  'bz-featured-tile.c',
  'bz-flathub-category.c',
  'bz-flathub-page.c',
  'bz-flathub-state.c',
  'bz-flatpak-entry.c',
  'bz-flatpak-instance.c',
  'bz-full-view.c',
  'bz-global-progress.c',
  'bz-global-state.c',
  'bz-gnome-shell-search-provider.c',
  'bz-group-tile-css-watcher.c',
  'bz-inhibited-scrollable.c',
  'bz-inspector.c',
  'bz-installed-page.c',
  'bz-io.c',
  'bz-issue.c',
  'bz-lazy-async-texture-model.c',
  'bz-patterned-background.c',
  'bz-preferences-dialog.c',
  'bz-progress-bar.c',
  'bz-release.c',
  'bz-releases-list.c',
  'bz-result.c',
  'bz-review.c',
  'bz-screenshot-dialog.c',
  'bz-screenshot.c',
  'bz-search-engine.c',
  'bz-search-result.c',
  'bz-search-widget.c',
  'bz-section-view.c',
  'bz-serializable.c',
  'bz-share-dialog.c',
  'bz-spdx.c',
  'bz-state-info.c',
  'bz-stats-dialog.c',
  'bz-transaction-entry-tracker.c',
  'bz-transaction-manager.c',
  'bz-transaction-task.c',
  'bz-transaction-view.c',
  'bz-transaction.c',
  'bz-update-dialog.c',
  'bz-url.c',
  'bz-window.c',
  'bz-world-map-parser.c',
  'bz-world-map.c',
  'bz-yaml-parser.c',
  'bz-zoom.c',
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
    'bz-app-size-dialog.blp',
    'bz-app-tile.blp',
    'bz-apps-page.blp',
    'bz-appstream-description-render.blp',
    'bz-browse-widget.blp',
    'bz-category-tile.blp',
    'bz-curated-app-tile.blp',
    'bz-decorated-screenshot.blp',
    'bz-detailed-app-tile.blp',
    'bz-entry-inspector.blp',
    'bz-featured-carousel.blp',
    'bz-featured-tile.blp',
    'bz-flathub-page.blp',
    'bz-full-view.blp',
    'bz-inspector.blp',
    'bz-installed-page.blp',
    'bz-preferences-dialog.blp',
    'bz-progress-bar.blp',
    'bz-releases-list.blp',
    'bz-releases-dialog.blp',
    'bz-screenshot-dialog.blp',
    'bz-search-widget.blp',
    'bz-section-view.blp',
    'bz-share-dialog.blp',
    'bz-stats-dialog.blp',
    'bz-transaction-view.blp',
    'bz-update-dialog.blp',
    'bz-window.blp',
    'gtk/shortcuts-dialog.blp',
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
