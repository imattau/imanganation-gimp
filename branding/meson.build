# Imanganation branding over GIMP's: the app icon (named "imanganation", as the desktop
# entry says), the About dialog logo and the splash.
#
# They live here rather than in the gimp-data submodule: that is GNOME's repository and
# builds fetch it from upstream, so changes there can't ship. gimp-data still builds
# GIMP's own logo and splash; install scripts (which run after everything is installed)
# put ours in their place.

branding_logo = files('imanganation-logo.svg')

# GIMP's extension panels use icon names resolved from the active GIMP icon
# theme. Keep these assets in the fork (not the upstream gimp-data submodule)
# so Flatpak builds can ship them without requiring a GNOME repository change.
panel_icons = files(
  'panel-icons/imanganation-bubbles.svg',
  'panel-icons/imanganation-bubbles-symbolic.svg',
  'panel-icons/imanganation-characters.svg',
  'panel-icons/imanganation-characters-symbolic.svg',
  'panel-icons/imanganation-context.svg',
  'panel-icons/imanganation-context-symbolic.svg',
  'panel-icons/imanganation-gallery.svg',
  'panel-icons/imanganation-gallery-symbolic.svg',
  'panel-icons/imanganation-pages.svg',
  'panel-icons/imanganation-pages-symbolic.svg',
  'panel-icons/imanganation-panel.svg',
  'panel-icons/imanganation-panel-symbolic.svg',
  'panel-icons/imanganation-project.svg',
  'panel-icons/imanganation-project-symbolic.svg',
  'panel-icons/imanganation-script.svg',
  'panel-icons/imanganation-script-symbolic.svg',
)
install_data(panel_icons,
             install_dir: gimpdatadir / 'icons' / 'Default' / 'scalable' / 'apps')

branding_copy = 'from pathlib import Path; from shutil import copy2; import sys, os; ' + \
                'target = Path(os.environ.get("MESON_INSTALL_DESTDIR_PREFIX")) / sys.argv[2]; ' + \
                'target.parent.mkdir(parents=True, exist_ok=True); copy2(sys.argv[1], target)'

foreach size : ['16', '22', '24', '32', '36', '48', '64', '72', '96', '128', '192', '256', '512']
  sizedir = size + 'x' + size
  icon = custom_target('imanganation' + sizedir + '.png',
                       input : branding_logo,
                       output: 'imanganation' + sizedir + '.png',
                       command: [ colorsvg2png, '@INPUT@', '@OUTPUT@', size ],
                       build_by_default: true,
                       install: false)
  meson.add_install_script(python, '-c', branding_copy, icon.full_path(),
                           get_option('datadir') / 'icons' / 'hicolor' / sizedir / 'apps' /
                           'imanganation.png')
endforeach

install_data(branding_logo,
             install_dir: get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
             rename: 'imanganation.svg')

# The About dialog's logo (gimp-data makes gimp-logo.png from Wilber).
about_logo = custom_target('imanganation-about-logo.png',
                           input : branding_logo,
                           output: 'imanganation-about-logo.png',
                           command: [ colorsvg2png, '@INPUT@', '@OUTPUT@', '128' ],
                           build_by_default: true,
                           install: false)
meson.add_install_script(python, '-c', branding_copy, about_logo.full_path(),
                         gimpdatadir / 'images' / 'gimp-logo.png')

# The splash (gimp-data renders gimp-splash.png from its .xcf).
meson.add_install_script(python, '-c', branding_copy,
                         meson.current_source_dir() / 'imanganation-splash.png',
                         gimpdatadir / 'images' / 'gimp-splash.png')
