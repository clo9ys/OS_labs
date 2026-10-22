#!/usr/bin/env bash
# Запуск Linux-окружения для лабораторных (нужен запущенный Docker Desktop).
#   ./env.sh            -> интерактивный bash внутри контейнера, репозиторий в /labs
#   ./env.sh <команда>  -> выполнить команду внутри контейнера
#   ./env.sh --rebuild  -> пересобрать образ
set -euo pipefail
cd "$(dirname "$0")"

IMAGE=os-labs:latest

if ! docker info >/dev/null 2>&1; then
    echo "Docker не отвечает: запусти Docker Desktop и повтори" >&2
    exit 1
fi

# `docker image inspect <имя>` в Docker Desktop (хранилище containerd) иногда
# ложно отвечает "No such image", хотя образ есть, поэтому ищем через список.
image_exists() {
    [[ -n "$(docker images -q "$1" 2>/dev/null)" ]]
}

build() {
    # если Docker Hub недоступен, а базовый rootfs импортирован локально — берём его
    local args=()
    image_exists ubuntu-base:24.04 && args=(--build-arg BASE=ubuntu-base:24.04)
    docker build ${args[@]+"${args[@]}"} -t "$IMAGE" .
}

# Сборка образа долгая, поэтому автоматически её не запускаем.
if [[ "${1:-}" == "--rebuild" ]]; then
    shift
    build
elif ! image_exists "$IMAGE"; then
    echo "Образ $IMAGE не найден. Собери его: ./env.sh --rebuild" >&2
    exit 1
fi

# -t только если stdin — терминал (иначе docker ругается при запуске из скриптов)
tty_flags=(-i)
[[ -t 0 ]] && tty_flags=(-it)

# SYS_PTRACE + seccomp=unconfined: strace работает через ptrace(2), а дефолтный
# seccomp-фильтр Docker может блокировать часть вызовов и искажать профиль.
exec docker run --rm "${tty_flags[@]}" \
    --hostname os-labs \
    --cap-add=SYS_PTRACE --security-opt seccomp=unconfined \
    -v "$PWD":/labs -w /labs \
    "$IMAGE" "${@:-bash}"
