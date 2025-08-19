# most of this was taken from the glycin source; please see
# https://gitlab.gnome.org/GNOME/glycin

deps = [
  gtk_dep,
]



cargo_bin = find_program('cargo')

cargo_target_dir = meson.project_build_root() / 'cargo-target'
cargo_home = meson.project_build_root() / 'cargo-home'
cargo_manifest = files('Cargo.toml')


cargo_env = environment()
cargo_env.set('CARGO_HOME', cargo_home)

if get_option('buildtype') == 'release'
  rust_target = 'release'
  cargo_profile = 'release'

  if get_option('lto') == 'auto'
    cargo_version_details = run_command(cargo_bin, 'version', '-v', check: true).stdout().strip()

    # COMMENT FROM GLYCIN: Disable LTO for architectures other than
    # x86_64 and aarch64 There have been frequent built issues with
    # memory limits on 32-bit architectures
    if not cargo_version_details.contains('host: x86_64-') and not cargo_version_details.contains('host: aarch64-')
      message('LTO automatically disabled')
      cargo_env.set('CARGO_PROFILE_RELEASE_LTO', 'false')
    endif
  elif get_option('lto') == 'false'
    cargo_env.set('CARGO_PROFILE_RELEASE_LTO', 'false')
  endif

else
  rust_target = 'debug'
  cargo_profile = 'dev'
endif


cargo_output = cargo_target_dir / rust_target / 'libbazaar_ui.a'

cargo_options = [
    '--target-dir', cargo_target_dir,
    '--profile', cargo_profile,
    '--manifest-path', cargo_manifest,
]



libbazaar_ui = custom_target(
  'bazaar-ui',
  build_always_stale: true,
  build_by_default: true,
  output: 'nothing',
  console: true,
  env: cargo_env,
  command: [cargo_bin, 'build', cargo_options, '-p', 'libbazaar-ui'],
)

# have to do this since meson doesn't let you do outputs in subdirs
bazaar_ui_copy = custom_target(
  'bazaar-ui-copy',
  depends: libbazaar_ui,
  build_by_default: true,
  build_always_stale: true,
  output: 'tmp.a',
  command: [
    'cp',
    '-a', cargo_output,
    meson.current_build_dir() / 'tmp.a',
  ],
)

bazaar_ui_dep = declare_dependency(
  sources: bazaar_ui_copy,
  include_directories: include_directories('include/'),
)
  
