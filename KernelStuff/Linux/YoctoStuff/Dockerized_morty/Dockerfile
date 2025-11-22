# docker build -t poky-builder-1604 .
# docker run -it --rm -v "$PWD:/workspace" poky-builder-1604:latest

FROM ubuntu:16.04

# Set noninteractive mode for apt to avoid prompts
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update 
RUN apt-get install -y \
    build-essential \
    chrpath \
    cpio \
    debianutils \
    diffstat \
    file \
    gawk \
    gcc \
    git \
    sudo \
    locales \
    python \
    python-pip \
    python-git \
    python-jinja2 \
    python-pexpect \
    python-subunit \
    python3 \
    python3-pip \
    python3-git \
    python3-jinja2 \
    python3-pexpect \
    python3-subunit \
    iputils-ping \
    libacl1 \
    liblz4-tool \
    socat \
    texinfo \
    unzip \
    wget \
    xz-utils \
    zstd

RUN apt-get clean

# Set locale
RUN locale-gen en_US.UTF-8
ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US:en
ENV LC_ALL=en_US.UTF-8

RUN useradd -m builder && echo 'builder ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers
USER builder

WORKDIR /resources

COPY poky.tar.gz .
COPY linux-yocto-4.4.tar.gz .
COPY yocto-kernel-cache.tar.gz .
COPY good_local.conf .
COPY build_commands.sh .
COPY macrod_4.4_recipe.bb .

WORKDIR /workspace

CMD ["/bin/bash"]
