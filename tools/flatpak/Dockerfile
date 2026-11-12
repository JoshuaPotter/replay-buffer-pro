# Ubuntu 24.04 x86_64 with flatpak-builder and a browser-based desktop, for building the
# Flathub extension and running it in Flatpak OBS on any machine with Docker. Flatpak
# needs bubblewrap's namespaces, so run.sh starts it with --privileged.
# See README.md next to this file.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
  && apt-get install -y --no-install-recommends \
    ca-certificates curl flatpak flatpak-builder git xz-utils \
    xvfb x11vnc novnc websockify openbox xterm dbus-x11 fonts-dejavu-core \
    python3 \
  && rm -rf /var/lib/apt/lists/*

COPY entrypoint.sh session.sh bwrap-no-seccomp /opt/rbp/
COPY --from=linux-desktop obs-config /opt/rbp/obs-config

EXPOSE 6080 4455
ENTRYPOINT ["/opt/rbp/entrypoint.sh"]
