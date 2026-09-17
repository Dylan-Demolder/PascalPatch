#!/usr/bin/env bash
set -euo pipefail

if command -v xdotool >/dev/null 2>&1; then
  echo "X11 input control: $(command -v xdotool)"
  xdotool -v
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
  install_with_sudo pacman -S --noconfirm --needed xdotool || true
elif command -v apt-get >/dev/null 2>&1; then
  install_with_sudo apt-get update || true
  install_with_sudo apt-get install -y xdotool || true
fi

command -v xdotool >/dev/null 2>&1 || {
  echo "Unable to install or resolve xdotool with the runner's available package manager and passwordless sudo" >&2
  exit 2
}

echo "X11 input control installed: $(command -v xdotool)"
xdotool -v
