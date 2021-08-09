## Summary
For FRocksDB-6.x, we need to release jar package which contains .so files for linux32 and linux64 (glibc and musl-libc), jnilib files for Mac OSX, and a .dll for Windows x64.

## Build in Windows

Use Windows 64 bit machine (e.g. base AWS Windows instance: 4 cores, 16GB RAM, 40GB storage for build).

Install:
 * git
 * java 8
 * maven
 * Visual Studio Community 15 (2017)

With [chocolatey](https://chocolatey.org/install):

    choco install git.install jdk8 maven visualstudio2017community

Optionally:

    choco install intellijidea-community vscode

Open developer command prompt for vs 2017 and run commands:

    git clone git@github.com:ververica/frocksdb.git
    cd frocksdb
    git checkout FRocksDB-6.20.3 # release branch
    java\crossbuild\build-win.bat

The result native library is `build\java\Release\rocksdbjni-shared.dll` and you can also find it under project foler with name `librocksdbjni-win64.dll`
The result windows jar is `build\java\rocksdbjni_classes.jar`.

There is also a how-to in CMakeLists.txt.

## Build in PPC64LE

### Build on a powerPC machine
Strongly suggest you to build on native powerPC machine:


### Build within a docker machine via QEMU
** warning ** It would be extremely slow to build within a docker machine via QEMU, from my experiense, it might need at least 8 hours to build FRocksDB once.

Use Ubuntu 16.04 (e.g. AWS instance 4 cores, 16GB RAM, 40GB storage for build).
Install git if not installed. If docker is installed, it might need to be removed.

Setup ppc64le docker machine ([source](https://developer.ibm.com/linuxonpower/2017/06/08/build-test-ppc64le-docker-images-intel/)):

    wget http://ftp.unicamp.br/pub/ppc64el/boot2docker/install.sh && chmod +x ./install.sh && ./install.sh -s
    docker-machine create -d qemu \
        --qemu-boot2docker-url=/home/ubuntu/.docker/machine/boot2docker.iso \
        --qemu-memory 8192 \
        --qemu-cpu-count 4 \
        --qemu-cache-mode none \
        --qemu-arch ppc64le \
        vm-ppc64le

Regenerate certs as suggested if it did not work at once.

Prepare docker machine to run rocksdbjni docker image for ppc64le build:

    eval $(docker-machine env vm-ppc64le)
    git clone git@github.com:ververica/frocksdb.git
    cd frocksdb
    git checkout FRocksDB-6.20.3 # release branch
    docker-machine ssh vm-ppc64le mkdir -p `pwd`
    docker-machine scp -r . vm-ppc64le:`pwd`

Build frocksdb:

    make jclean clean rocksdbjavastaticdockerppc64le
    docker-machine scp vm-ppc64le:`pwd`/java/target/librocksdbjni-linux-ppc64le.so java/target/.
    make jclean clean rocksdbjavastaticdockerppc64lemusl
    docker-machine scp vm-ppc64le:`pwd`/java/target/librocksdbjni-linux-ppc64le.so java/target/.

The result native libraries are `java/target/librocksdbjni-linux-ppc64le.so` and `java/target/librocksdbjni-linux-ppc64le-musl.so`.

## Final crossbuild in Mac OSX

Read how to Build cross jar for Mac OSX and linux as described in java/RELEASE.md but do not run it yet.

Run commands:

    make jclean clean
    mkdir -p java/target
    cp <path-to-windows-dll>/librocksdbjni-win64.dll java/target/librocksdbjni-win64.dll
    cp <path-to-ppc64le-lib-so>/librocksdbjni-linux-ppc64le.so java/target/librocksdbjni-linux-ppc64le.so
    cp <path-to-ppc64le-musl-lib-so>/librocksdbjni-linux-ppc64le-musl.so java/target/librocksdbjni-linux-ppc64le-musl.so
    FROCKSDB_VERSION=1.0 PORTABLE=1 ROCKSDB_DISABLE_JEMALLOC=true make clean frocksdbjavastaticreleasedocker

* Note, we disable jemalloc on mac due to https://github.com/facebook/rocksdb/issues/5787

## Push to maven central

Run:
```bash
VERSION=<release version> \
USER=<sonatype user> \
PASSWORD=<sonatype password> \
KEYNAME=<key name> \
PASSPHRASE=<passphrase> \
java/publish-frocksdbjni.sh
```

Go to the staging repositories on Sonatype:

https://oss.sonatype.org/#stagingRepositories

Select the open staging repository and click on "Close".

Test the files in the staging repository 
which will look something like this `https://oss.sonatype.org/content/repositories/xxxx-1020`.

Press the "Release" button (WARNING: this can not be undone).