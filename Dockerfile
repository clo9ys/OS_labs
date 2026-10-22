ARG BASE=ubuntu:24.04
FROM ${BASE}

ARG PACKAGES="build-essential gdb strace ltrace \
              man-db manpages manpages-dev manpages-posix-dev \
              procps psmisc file less nano"

RUN rm -f /etc/dpkg/dpkg.cfg.d/excludes \
 && echo 'Acquire::Retries "10"; Acquire::http::Timeout "30";' > /etc/apt/apt.conf.d/80retries \
 && apt-get update \
 && for i in 1 2 3 4 5 6 7 8; do \
        DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends $PACKAGES && break; \
        echo "apt attempt $i failed, retrying"; sleep 5; \
    done \
 && dpkg -s strace gcc make >/dev/null \
 && rm -rf /var/lib/apt/lists/* /var/cache/apt/archives/*.deb

WORKDIR /labs
CMD ["bash"]
