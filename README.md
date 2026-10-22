# Лабораторные работы по курсу «Операционные системы»

Каждая лабораторная лежит в своей директории:

```
labN/
├── src/      исходный код (+ Makefile)
└── report/   отчёт в формате PDF
```

| ЛР | Тема |
|----|------|
| [lab2](lab2) | Процессы и каналы (pipe), вариант 22 |
| [lab3](lab3) | Потоки и синхронизация (pthread), вариант 17 |

## Окружение

Лабораторные выполняются под Linux (Ubuntu 24.04 LTS, aarch64) в Docker-контейнере:

```bash
./env.sh                 # интерактивный bash в контейнере, репозиторий смонтирован в /labs
./env.sh make -C lab2/src
./env.sh make -C lab3/src
./env.sh --rebuild       # пересобрать образ
```

Контейнер запускается с `--cap-add=SYS_PTRACE --security-opt seccomp=unconfined`:
strace работает через `ptrace(2)`, а стандартный seccomp-профиль Docker может
запрещать часть системных вызовов и искажать профиль.

### Если Docker Hub недоступен

Базовый образ можно собрать из официального rootfs Ubuntu:

```bash
curl -O http://cdimage.ubuntu.com/ubuntu-base/releases/24.04/release/ubuntu-base-24.04.5-base-arm64.tar.gz
docker import --platform linux/arm64 --change 'CMD ["/bin/bash"]' \
    ubuntu-base-24.04.5-base-arm64.tar.gz ubuntu-base:24.04
./env.sh --rebuild       # env.sh сам подхватит локальный ubuntu-base:24.04
```
