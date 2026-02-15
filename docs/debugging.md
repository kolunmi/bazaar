# Debugging Bazaar

## Running Bazaar with Debug Messages Enabled

You do this the same as with any other GLib application:

```sh
pkill bazaar; env G_MESSAGES_DEBUG=all bazaar
```

This will produce a flood of output. If you know the specific log domain you
want to see, you can replace `all` with that domain name, such as `flatpak` or
`BAZAAR::CORE`.

## Bazaar Inspector

Bazaar has its own inspector window, which you can spawn with the key combo
`control+alt+shift+i`. This is meant to be useful for developers and
distributors.

The global debug mode toggle in this window basically just enables a bottom bar
in each Bazaar window which displays useful information particular to that
window and allows you to easily inspect the UI entry for the visible
application.

You can also disable blocklists from this window. This is a temporary internal
property, not a configuration option; It will not be saved for the next Bazaar
process.

The rest of the contents of this window should be self-explanatory.
