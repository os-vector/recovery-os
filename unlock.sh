#!/bin/bash

usage() {
    echo "$1"
    echo "Usage: ./unlock.sh -bt <dev/oskr> -ap <ABOOT-pw> -op <OTA-pw> -bp <OSKR-boot-passwd> -q <QSN> -e <ESN>"
    exit 1
}


while [ $# -gt 0 ]; do
    case "$1" in
        -bt) BOT_TYPE="$2"; shift ;;
        -ap) ABOOT_SIGNING_PASSWORD="$2"; shift ;;
        -op) OTA_SIGNING_KEY_PASSWORD="$2"; shift ;;
        -bp) OSKR_BOOT_PASSWORD="$2"; shift ;;
        -q) QSN="$2"; shift ;;
        -e) ESN="$2"; shift ;;
        *)
            usage "unknown option: $1"
            exit 1 ;;
    esac
    shift
done

if [[ "$BOT_TYPE" != "oskr" && "$BOT_TYPE" != "dev" ]]; then
    usage "BOT_TYPE should be 'oskr' or 'dev', got: $BOT_TYPE"
    exit 1
fi

# making sure everything's set nyaaa
[[ -z "$ABOOT_SIGNING_PASSWORD" ]] && { usage "missing ABOOT pw"; exit 1; }
[[ -z "$OTA_SIGNING_KEY_PASSWORD" ]] && { usage "missing OTA pw"; exit 1; }
[[ "$BOT_TYPE" == "oskr" && -z "$OSKR_BOOT_PASSWORD" ]] && { usage "OSKR needs boot passwd"; exit 1; }
[[ -z "$QSN" ]] && { usage "missing QSN"; exit 1; }
[[ -z "$ESN" ]] && { usage "missing ESN"; exit 1; }

if ! command -v docker >/dev/null 2>&1; then
    echo "docker not found, install it."
    exit 1
fi


if ! docker run --rm hello-world >/dev/null 2>&1; then
    echo "docker isn't working properly."
    exit 1
fi

#mkdir -p "$(pwd)/oskr"

#if [ ! -d "$(pwd)" ]; then
#    echo "$(pwd)/oskr not found."
#    exit 1
#fi

#if [ -f "$(pwd)/oskr/OSKR-unlock.tar.gz" ]; then
#    if [ ! -d "$(pwd)/oskr/vicos-oelinux" ]; then
#        echo "extracting OSKR-unlock.tar.gz, nyaa..."
#        tar -xzvf "$(pwd)/oskr/OSKR-unlock.tar.gz" -C "$(pwd)/oskr"
#    else
#        echo "vicos-oelinux already exists, skipping extraction."
#    fi
#else
#    echo "$(pwd)/oskr/OSKR-unlock.tar.gz not found."
#    exit 1
#fi

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

if [[ $BOT_TYPE == "oskr" ]]; then
    export EXPORTBTCOMMAND="export OSKR=1"
else
    export EXPORTBTCOMMAND="export DEV=1"
fi

echo "starting docker container..."
docker run -it -v "$(pwd):/home/build" oskr-ubuntu16 bash -c "
exec bash --rcfile <(echo \"cd ~/vicos-oelinux/poky && mkdir -p ../../final-unlock-otas && source build/conf/set_bb_env.sh && bitbake -c cleanall rampost victor && rm -rf sstate-cache/0/sstate\:victor\:* && export -n FACTORY && export -n OSKR && $EXPORTBTCOMMAND && build-victor-robot-factory-image && cd ~/vicos-oelinux/ota && export ABOOT_SIGNING_PASSWORD=$ABOOT_SIGNING_PASSWORD && export OTA_MANIFEST_SIGNING_PASSWORD=$OTA_SIGNING_KEY_PASSWORD && export BOOT_IMAGE_SIGNING_PASSWORD=$OSKR_BOOT_PASSWORD && make ${BOT_TYPE}sign && unset_bb_env && export DISTRO=msm-perf && export VARIANT=perf && export PRODUCT=robot && $EXPORTBTCOMMAND && export -n FACTORY && export && python3 mk_unlock.py -q $QSN -e $ESN -sm --sectools /home/build/apq8009-le-1-0-2_ap_standard_oem/common/tools/sectools && make verify-aboot-$BOT_TYPE && make verify-boot-$BOT_TYPE && echo && echo && cp ../_build/unlock/$ESN.ota ../../final-unlock-otas/ && echo Unlock OTA created. && exit 0\")
"
