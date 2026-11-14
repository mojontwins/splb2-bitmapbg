#!/bin/sh
# roomed - build on Linux / BSD / macOS (XQuartz). Needs a C compiler and the
# X11 headers (libx11-dev). Produces ./roomed.
#   ./build.sh            the editor
#   ./build.sh headless   roomed-headless, the scripted test build
cd "$(dirname "$0")"
make ${1:-roomed}
