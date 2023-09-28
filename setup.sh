#!/bin/bash

case "$1" in
pacman)
    echo "=======Build Deps======"
    sudo pacman -S make cmake ninja pkgconf gcc
    echo "=======KDE specific build deps======"
    sudo pacman -S extra-cmake-modules || echo "Is this KDE?"
    echo "=======Runtime Deps======"
    sudo pacman -S libnm json-glib python python-pydbus python-netifaces
    echo "=======Dev deps======"
    sudo pacman -S nm-connection-editor jq
    echo "=======Optional packaging deps======"
    sudo pacman -S dpkg rpmbuild
    ;;
apt)   echo "=======Build Deps======"
    sudo apt install make cmake ninja-build pkgconf gcc
    echo "=======KDE specific build deps======"
    sudo apt install extra-cmake-modules || echo "Is this KDE?"
    echo "=======Runtime Deps======"
    sudo apt install libnm-dev python3-dbus python3-netifaces
    echo "=======Dev deps======"
    sudo apt install nm-connection-editor jq
    echo "=======Optional packaging deps======"
    sudo apt install rpmbuild pacman
    ;;
yum)
    echo "=======Build Deps======"
    sudo yum install make cmake ninja-build pkgconf gcc-c++ NetworkManager-libnm-devel json-glib-devel gtk3-devel gtk4-devel
    echo "=======KDE specific build deps======"
    sudo yum install extra-cmake-modules || echo "Is this KDE?"
    echo "=======Runtime Deps======"
    sudo yum install NetworkManager-libnm gtk3 gtk4 json-glib python python-pydbus python-netifaces
    echo "=======Dev deps======"
    sudo yum install nm-connection-editor jq
    echo "=======Optional packaging deps======"
    sudo yum install rpm-build dpkg pacman
    ;;
*)
    echo "Usage: $0 <pacman|apt|yum>"
    exit 1
    ;;
esac
