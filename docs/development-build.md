# Build and run the downstream GIMP fork

The fork is a normal Meson/Ninja GIMP build. Keep the build directory outside the
source checkout so generated files and local dependencies do not enter commits.

## Configure and compile

With Meson, Ninja, GEGL development files and the remaining GIMP dependencies
installed, run from the fork checkout:

```sh
meson setup /tmp/imanganation-gimp-build . \
  --prefix="$HOME/.local/gimp-imanganation" \
  --pkg-config-path="$HOME/.local/gimp-deps/lib/x86_64-linux-gnu/pkgconfig:$HOME/.local/gimp-deps/lib/pkgconfig:$HOME/.local/gimp-deps/usr/lib/x86_64-linux-gnu/pkgconfig:$HOME/.local/gimp-deps/usr/share/pkgconfig"
ninja -C /tmp/imanganation-gimp-build
```

If you already configured that directory, use `meson setup --reconfigure` to update
it. The shared-library path must include the user-local dependency prefix when running
the uninstalled binaries:

```sh
export PATH="$HOME/.local/gimp-deps/usr/bin:$PATH"
export LD_LIBRARY_PATH="$HOME/.local/gimp-deps/lib/x86_64-linux-gnu:$HOME/.local/gimp-deps/lib:$HOME/.local/gimp-deps/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

## Launch with the Imanganation Python plug-in

Set `GIMP3_DIRECTORY` to a disposable profile and install the plug-in in that
profile's `plug-ins` directory. The profile keeps test preferences and dock layout
separate from the user's normal GIMP settings.

```sh
export GIMP3_DIRECTORY="$HOME/.config/GIMP/imanganation-dev"
mkdir -p "$GIMP3_DIRECTORY/plug-ins/imanganation"
ln -s "$(realpath ../gimp/imanganation/imanganation.py)" \
  "$GIMP3_DIRECTORY/plug-ins/imanganation/imanganation.py"
ln -s "$(realpath ../gimp/imanganation/project_store.py)" \
  "$GIMP3_DIRECTORY/plug-ins/imanganation/project_store.py"
ln -s "$(realpath ../gimp/imanganation/panel_ui.py)" \
  "$GIMP3_DIRECTORY/plug-ins/imanganation/panel_ui.py"
ln -s "$(realpath ../gimp/imanganation/placement.py)" \
  "$GIMP3_DIRECTORY/plug-ins/imanganation/placement.py"
export GIMP3_SYSCONFDIR="$(realpath etc)"
chmod +x "$GIMP3_DIRECTORY/plug-ins/imanganation/imanganation.py"
/tmp/imanganation-gimp-build/app/gimp-3.3
```

`GIMP3_SYSCONFDIR` makes the uninstalled build read the fork's defaults: `etc/sessionrc`
(the workspace layout: Project navigator and toolbox left, Context with Script and Page
Filmstrip tabs above Layers right) and `etc/toolrc` (a compact toolbox: Select, Move,
Transform, Brush, Erase, Fill, Text, Colour picker; other tools stay in the Tools menu).
They apply only to a profile without its own `sessionrc`/`toolrc`; delete those files, or
use *Preferences → Interface → Window Management → Reset Saved Window Positions*, to
return an existing profile to them. An extension dock the layout leaves out stays closed
until opened from the plug-in (e.g. *Windows → Imanganation*).

Launch from the `imanganation-gimp` checkout so the relative path above resolves to
the sibling main repository. GIMP must be built with Python plug-in support; its
plug-in console or `--verbose` output reports plug-in discovery and import failures.
For a command-line/headless invocation, use `app/gimp-console-3.3` with the same
environment. The C sample is registered by the persistent PDB procedure
`extension-imanganation-panels`. Invoke it from the Procedure Browser or a Python-Fu
console. The Python example in the sibling repository is
`gimp/extension_panel_python_sample.py`; install it as an executable plug-in to check
Python-owned registration and action callbacks against this build.

The current developer build directory is `/tmp/imanganation-gimp-build`; it is local
and disposable, not part of the repository. Re-run the setup command to reproduce it.
