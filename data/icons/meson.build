application_id = 'io.github.kolunmi.Bazaar'

scalable_dir = 'hicolor' / 'scalable' / 'apps'
if get_option('development')
  icon_file = '@0@.Devel.svg'.format(application_id)
else
  icon_file = '@0@.svg'.format(application_id)
endif

install_data(
  scalable_dir / icon_file,
  install_dir: get_option('datadir') / 'icons' / scalable_dir,
  rename: '@0@.svg'.format(application_id)
)

symbolic_dir = 'hicolor' / 'symbolic' / 'apps'
install_data(
  symbolic_dir / ('@0@-symbolic.svg').format(application_id),
  install_dir: get_option('datadir') / 'icons' / symbolic_dir
)
