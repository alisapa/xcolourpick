# xcolourpick

xcolourpick is an X11 utility to print out the colour of any pixel on the
screen.

For usage instructions, please refer to [the man page](xcolourpick.1).
It can be rendered without installing xcolourpick by invoking one of the
following commands:

```sh
man ./xcolourpick.1
```

```sh
groff -Tascii -man xcolourpick.1
```

## Installation

The only dependency is Xlib. It is available as `libX11` on Gentoo, and
as `libx11-dev` on Debian.

To compile xcolourpick:

```sh
make
```

To install (omit `PREFIX` option to install to `/usr/local/`):

```sh
make install PREFIX=/path/to/prefix
```
