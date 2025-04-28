#!/bin/bash

# JUST for creating a prod recovery, no ABOOT
# used by Wire for re-locking bots

usage() {
    echo "$1"
    echo "Usage: ./make-prod-factory.sh <prod-boot-passwd>"
    echo "HINT: same as ABOOT passwd"
    exit 1
}

BOT_TYPE="prod"
BOOT_PASSWORD=$1

[[ -z "$BOOT_PASSWORD" ]] && { usage "prod needs boot passwd"; exit 1; }

# ensure we're on ubuntu/debian
#if [ ! -f /etc/debian_version ]; then
#    echo "sorry, this script only runs on ubuntu/debian"
#    exit 1
#fi

if ! command -v docker >/dev/null 2>&1; then
    echo "docker not found, install it."
    exit 1
fi


if ! docker run --rm hello-world >/dev/null 2>&1; then
    echo "docker isn't working properly."
    exit 1
fi

if [[ ! -d ../vector-oskr-unlock/vicos-oelinux ]]; then
    echo "run this script in the vector-oskr-unlock folder."
    exit 1
fi

if [[ -d anki-deps ]]; then
    mv anki-deps .anki
fi

echo "setting wwise symlink"

ORIGDIR="$(pwd)"
cd "$(pwd)/vicos-oelinux/anki/victor/lib/audio/wwise/versions"
rm -f current
ln -sf /home/build/.anki/wwise/versions/2017.2.7_a current
cd "$ORIGDIR"

if [[ $1 == "-c" ]]; then
    echo "cleaning yocto build..."
    rm -rf "$(pwd)/oskr/vicos-oelinux/poky/build/tmp-glibc" "$(pwd)/oskr/vicos-oelinux/poky/build/cache" "$(pwd)/oskr/vicos-oelinux/poky/build/sstate-cache"
fi

echo "building docker image (oskr-ubuntu16)..."
docker build -t oskr-ubuntu16 .

#echo "starting docker container..."
#docker run -it -v "$(pwd):/home/build" oskr-ubuntu16 bash -c '
#exec bash --rcfile <(echo "cd ~/vicos-oelinux/poky && source build/conf/set_bb_env.sh && build-victor-robot-factory-image && cd ~/vicos-oelinux/ota && make oskrsign && export OSKR=1 && export -n FACTORY && python3 mk_unlock.py -q $QSN -e $ESN -sm --sectools /home/build/apq8009-le-1-0-2_ap_standard_oem/common/tools/sectools")
#'

# if [[ $BOT_TYPE == "oskr" ]]; then
#     export EXPORTBTCOMMAND="export OSKR=1"
# else
#     export EXPORTBTCOMMAND="export DEV=1"
# fi
export EXPORTBTCOMMAND="export PROD=1"

echo "starting docker container..."
docker run -it -v "$(pwd):/home/build" oskr-ubuntu16 bash -c "
exec bash --rcfile <(echo \"cd ~/vicos-oelinux/poky && mkdir -p ../../final-unlock-otas && source build/conf/set_bb_env.sh && bitbake -c cleanall rampost victor && rm -rf sstate-cache/0/sstate\:victor\:* && export -n FACTORY && export -n OSKR && $EXPORTBTCOMMAND && build-victor-robot-factory-image && cd ~/vicos-oelinux/ota && export BOOT_IMAGE_SIGNING_PASSWORD=$BOOT_PASSWORD && make ${BOT_TYPE}sign && echo prod thing created && exit 0\")
"
