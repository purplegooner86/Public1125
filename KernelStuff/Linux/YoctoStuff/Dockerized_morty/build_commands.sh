if [ -z "$1" ] 
    then
        echo "Usage: ./build_commands.sh <minor version>"
        exit 1
fi

set -e

MY_MINOR_VERSION="$1"

git config --global user.email "no@thanks.com"
git config --global user.name "Autobuilder"

tar -xzvf /resources/linux-yocto-4.4.tar.gz
tar -xzvf /resources/poky.tar.gz
tar -xzvf /resources/yocto-kernel-cache.tar.gz

cd yocto-kernel-cache
git checkout yocto-4.4
rm -rf ./.git
cd ../
cp -r yocto-kernel-cache yocto-kernel-cache-4.4
rm -rf yocto-kernel-cache

cd linux-yocto-4.4
git checkout "v4.4.${MY_MINOR_VERSION}"
rm -rf ./.git
cd ../
cp -r linux-yocto-4.4 "linux-yocto-4.4.${MY_MINOR_VERSION}"
rm -rf linux-yocto-4.4

cd yocto-kernel-cache-4.4
git init .
git add *
git commit -m "initial commit"
KERNEL_CACHE_REV=$(git rev-parse HEAD)
cd ../

cd "linux-yocto-4.4.${MY_MINOR_VERSION}"
git init .
git add *
git commit -m "initial commit"
KERNEL_SRC_REV=$(git rev-parse HEAD)
cd ../

echo "MY_MINOR_VERSION = \"${MY_MINOR_VERSION}\"" | cat - /resources/good_local.conf > ./my_local.conf

sed "s/MACROMACROMACRO_MACHINE_HASH/${KERNEL_SRC_REV}/g" /resources/macrod_4.4_recipe.bb > "./linux-yocto-4.4.${MY_MINOR_VERSION}.bb"
sed -i "s/MACROMACROMACRO_META_HASH/${KERNEL_CACHE_REV}/g" "./linux-yocto-4.4.${MY_MINOR_VERSION}.bb"
sed -i "s/MACROMACROMACRO_MINOR_VERSION/${MY_MINOR_VERSION}/g" "./linux-yocto-4.4.${MY_MINOR_VERSION}.bb"

cd poky
git checkout morty
source oe-init-build-env
devtool create-workspace
mkdir -p "workspace/recipes/linux-yocto-4.4.${MY_MINOR_VERSION}"
cp "/workspace/linux-yocto-4.4.${MY_MINOR_VERSION}.bb" "./workspace/recipes/linux-yocto-4.4.${MY_MINOR_VERSION}/"
cp /workspace/my_local.conf ./conf/local.conf

bitbake virtual/kernel
