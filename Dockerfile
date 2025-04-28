FROM ubuntu:16.04
RUN apt-get update && apt-get install -y sudo
RUN apt-get update && apt-get install -y \
    build-essential chrpath cpio debianutils diffstat file gawk gcc git iputils-ping libacl1 liblz4-tool locales python3 python3-git python3-jinja2 python3-pexpect python3-subunit socat texinfo unzip wget xz-utils zstd
RUN apt-get update && apt-get install -y \
    git-core gnupg flex bison gperf build-essential zip curl zlib1g-dev gcc-multilib g++-multilib libc6-dev-i386 lib32ncurses5-dev x11proto-core-dev libx11-dev lib32z-dev libxml-simple-perl libc6-dev libgl1-mesa-dev tofrodos python-markdown libxml2-utils xsltproc genisoimage
RUN apt-get update && apt-get install -y \
    gawk chrpath texinfo p7zip-full android-tools-fsutils
RUN apt-get update && apt-get install -y \
    ruby ninja-build subversion libssl-dev
RUN ln -sf /bin/bash /bin/sh
RUN useradd -ms /bin/bash build && echo "build ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
USER build
WORKDIR /home/build
CMD ["/bin/bash"]
