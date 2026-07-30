#!/usr/bin/env bash
# cspell:disable

set -o errexit
set -o pipefail
set -o nounset
set -o xtrace

command="${1:-help}"

irqaffinity="0-2,6-8"
isolcpus="3-5,9-11"
config="isolcpus=managed_irq,domain,${isolcpus} nohz_full=${isolcpus} rcu_nocbs=${isolcpus} irqaffinity=${irqaffinity}"

usage() {
    echo "Usage: $0 {show|add|remove|help}"
    exit 1
}

check_root() {
    if [ "$(id -u)" -ne 0 ]; then
        echo "This command requires root privileges. Please run with sudo." >&2
        exit 1
    fi
}

case "${command}" in
    show)
        lscpu --extended
        cat /proc/cmdline
        check_root
        kernelstub --print-config | grep -f "Kernel Boot Options:"
        ;;
    add)
        check_root
        kernelstub --add-option "${config}"
        ;;
    remove)
        check_root
        kernelstub --delete-option "${config}"
        ;;
    help)
        usage
        ;;
    *)
        usage
        ;;
esac
