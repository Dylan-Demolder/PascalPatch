#!/usr/bin/env bash
set -euo pipefail

if command -v xdotool >/dev/null 2>&1 && command -v qtpaths >/dev/null 2>&1; then
  echo "X11 input control: $(command -v xdotool)"
  xdotool -v
  echo "Qt paths: $(command -v qtpaths)"
  qtpaths --version
  exit 0
fi

install_with_sudo() {
  if command -v sudo >/dev/null 2>&1 && sudo -n true >/dev/null 2>&1; then
    sudo -n "$@"
    return 0
  fi
  return 1
}

if command -v pacman >/dev/null 2>&1; then
  install_with_sudo pacman -S --noconfirm --needed xdotool qt5-tools || true
elif command -v apt-get >/dev/null 2>&1; then
  install_with_sudo apt-get update || true
  install_with_sudo apt-get install -y xdotool qttools5-dev-tools || true
fi

command -v xdotool >/dev/null 2>&1 && command -v qtpaths >/dev/null 2>&1 || {
  echo "Unable to install or resolve xdotool and qtpaths with the runner's available package manager and passwordless sudo" >&2
  exit 2
}

echo "X11 input control installed: $(command -v xdotool)"
xdotool -v
echo "Qt paths installed: $(command -v qtpaths)"
qtpaths --version
