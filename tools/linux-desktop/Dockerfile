# Ubuntu 24.04 or 26.04 x86_64 desktop for testing the Linux build on any machine with
# Docker. It has OBS Studio from the official PPA, a browser-based desktop (Xvfb,
# Openbox, x11vnc, noVNC), and the packages .github/scripts/build-ubuntu needs, so the
# same image builds the plugin's .deb and runs it. run.sh builds it for linux/amd64.
# See README.md next to this file.
ARG UBUNTU=26.04
FROM ubuntu:${UBUNTU}

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
  && apt-get install -y --no-install-recommends ca-certificates gpg-agent software-properties-common \
  && add-apt-repository -y ppa:obsproject/obs-studio \
  && apt-get update \
  && apt-get install -y --no-install-recommends \
    obs-studio \
    xvfb x11vnc novnc websockify openbox xterm dbus-x11 fonts-dejavu-core libgl1-mesa-dri \
    ffmpeg sudo \
    build-essential cmake ninja-build pkg-config ccache git jq zsh file \
    libgles2-mesa-dev libsimde-dev qt6-base-dev qt6-base-private-dev libqt6svg6-dev \
    libavformat-dev libavcodec-dev libavutil-dev libcurl4-openssl-dev \
  && rm -rf /var/lib/apt/lists/*

# OBS runs as the image's own "ubuntu" user, so its settings land in ~/.config/obs-studio
# as on a real install. sudo lets you test `sudo apt remove replay-buffer-pro` from xterm.
RUN echo 'ubuntu ALL=(ALL) NOPASSWD:ALL' > /etc/sudoers.d/ubuntu \
  && git config --system --add safe.directory /src

COPY entrypoint.sh session.sh /opt/rbp/
COPY obs-config /opt/rbp/obs-config

EXPOSE 6080 4455
ENTRYPOINT ["/opt/rbp/entrypoint.sh"]
