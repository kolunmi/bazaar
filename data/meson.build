desktop_file = i18n.merge_file(
        input: 'io.github.kolunmi.Bazaar.desktop.in',
       output: 'io.github.kolunmi.Bazaar.desktop',
         type: 'desktop',
       po_dir: '../po',
      install: true,
  install_dir: get_option('datadir') / 'applications'
)

desktop_utils = find_program('desktop-file-validate', required: false)
if desktop_utils.found()
  test('Validate desktop file', desktop_utils, args: [desktop_file])
endif

appstream_file = i18n.merge_file(
        input: 'io.github.kolunmi.Bazaar.metainfo.xml.in',
       output: 'io.github.kolunmi.Bazaar.metainfo.xml',
       po_dir: '../po',
      install: true,
  install_dir: get_option('datadir') / 'metainfo'
)

appstreamcli = find_program('appstreamcli', required: false, disabler: true)
test('Validate appstream file', appstreamcli,
     args: ['validate', '--no-net', '--explain', appstream_file])

install_data('io.github.kolunmi.Bazaar.gschema.xml',
  install_dir: get_option('datadir') / 'glib-2.0' / 'schemas'
)

compile_schemas = find_program('glib-compile-schemas', required: false, disabler: true)
test('Validate schema file',
     compile_schemas,
     args: ['--strict', '--dry-run', meson.current_source_dir()])


service_conf = configuration_data()
service_conf.set('bindir', get_option('prefix') / get_option('bindir'))
configure_file(
  input: 'io.github.kolunmi.Bazaar.service.in',
  output: 'io.github.kolunmi.Bazaar.service',
  configuration: service_conf,
  install_dir: get_option('prefix') / 'lib' / 'systemd' / 'user'
)

service_conf = configuration_data()
configure_file(
  input: 'io.github.kolunmi.Bazaar.search-provider.ini.in',
  output: 'io.github.kolunmi.Bazaar.search-provider.ini',
  configuration: service_conf,
  install_dir: get_option('datadir') / 'gnome-shell' / 'search-providers'
)

subdir('icons')
subdir('dbus')
