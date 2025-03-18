#!/bin/bash
# Author: Kang Lin <kl222@126.com>

#See: https://blog.csdn.net/alwaysbefine/article/details/114187380
#set -x
set -e
#set -v


if [ -z "$BUILD_VERBOSE" ]; then
    BUILD_VERBOSE=OFF
fi

usage_long() {
    echo "$0 [-h|--help] [-v|--verbose[=0|1]] [--install=<install directory>]"
    echo "  --help|-h: Show help"
    echo "  -v|--verbose: Show build verbose"
    echo "  --install: Set depend libraries install directory"
}

# [如何使用getopt和getopts命令解析命令行选项和参数](https://zhuanlan.zhihu.com/p/673908518)
# [【Linux】Shell命令 getopts/getopt用法详解](https://blog.csdn.net/arpospf/article/details/103381621)
if command -V getopt >/dev/null; then
    echo "getopt is exits"
    #echo "original parameters=[$@]"
    # -o 或 --options 选项后面是可接受的短选项，如 ab:c:: ，表示可接受的短选项为 -a -b -c ，
    # 其中 -a 选项不接参数，-b 选项后必须接参数，-c 选项的参数为可选的
    # 后面没有冒号表示没有参数。后跟有一个冒号表示有参数。跟两个冒号表示有可选参数。
    # -l 或 --long 选项后面是可接受的长选项，用逗号分开，冒号的意义同短选项。
    # -n 选项后接选项解析错误时提示的脚本名字
    OPTS=help,verbose::,install:
    ARGS=`getopt -o h,v:: -l $OPTS -n $(basename $0) -- "$@"`
    if [ $? != 0 ]; then
        echo "exec getopt fail: $?"
        exit 1
    fi
    #echo "ARGS=[$ARGS]"
    #将规范化后的命令行参数分配至位置参数（$1,$2,......)
    eval set -- "${ARGS}"
    #echo "formatted parameters=[$@]"

    while [ $1 ]
    do
        #echo "\$1: $1"
        #echo "\$2: $2"
        case $1 in
        --install)
            INSTALL_DIR=$2
            shift 2
            ;;
        -v |--verbose)
            case $2 in
                "")
                    BUILD_VERBOSE=ON;;
                *)
                    BUILD_VERBOSE=$2;;
            esac
            shift 2
            ;;
        --) # 当解析到“选项和参数“与“non-option parameters“的分隔符时终止
            shift
            break
            ;;
        -h | -help)
            usage_long
            shift
            ;;
        *)
            usage_long
            break
            ;;
        esac
    done
fi

# store repo root as variable
REPO_ROOT=$(readlink -f $(dirname $(dirname $(readlink -f $0))))
OLD_CWD=$(readlink -f .)

pushd "$REPO_ROOT"

if [ -n "$1" -a -z "$QT_ROOT" ]; then
	QT_ROOT=$1
fi

if [ -d "/usr/lib/`uname -m`-linux-gnu" -a -z "$QT_ROOT" ]; then
    QT_ROOT="/usr/lib/`uname -m`-linux-gnu"
fi

if [ -z "$QT_ROOT" ]; then
    echo "QT_ROOT=$QT_ROOT"
	echo "$0 QT_ROOT RabbitCommon_ROOT"
    exit 1
fi

if [ -n "$2" -a -z "$RabbitCommon_ROOT" ]; then
	RabbitCommon_ROOT=$2
fi

if [ -z "$RabbitCommon_ROOT" ]; then
	RabbitCommon_ROOT=`pwd`/../RabbitCommon
    echo "RabbitCommon_ROOT=$RabbitCommon_ROOT"
fi

if [ ! -d "$RabbitCommon_ROOT" ]; then
    echo "QT_ROOT=$QT_ROOT"
    echo "RabbitCommon_ROOT=$RabbitCommon_ROOT"
	echo "$0 QT_ROOT RabbitCommon_ROOT"
    exit 2
fi

export QT_ROOT=$QT_ROOT
export RabbitCommon_ROOT=$RabbitCommon_ROOT

if [ -z "$BUILD_TYPE" ]; then
    export BUILD_TYPE=Release
fi

export PATH=$QT_ROOT/bin:$PATH
if [ -n "$INSTALL_DIR" ]; then
    export INSTALL_DIR=$INSTALL_DIR
fi

#fakeroot debian/rules binary

# -p, --sign-command=sign-command
#  When dpkg-buildpackage needs to execute GPG to sign a source
#  control (.dsc) file or a .changes file it will run sign-command
#  (searching the PATH if necessary) instead of gpg (long option since
#  dpkg 1.18.8).  sign-command will get all the arguments that gpg
#  would have gotten. sign-command should not contain spaces or any
#  other shell metacharacters.

# -k, --sign-key=key-id
#  Specify a key-ID to use when signing packages (long option since
#  dpkg 1.18.8).

# -us, --unsigned-source
#  Do not sign the source package (long option since dpkg 1.18.8).

# -ui, --unsigned-buildinfo
#  Do not sign the .buildinfo file (since dpkg 1.18.19).

# -uc, --unsigned-changes
#  Do not sign the .buildinfo and .changes files (long option since
#  dpkg 1.18.8).

# -b  Equivalent to --build=binary or --build=any,all.
# -S  Equivalent to --build=source
# -d, --no-check-builddeps    do not check build dependencies and conflicts.
#      --ignore-builtin-builddeps
#                              do not check builtin build dependencies.

#The -us -uc tell it there is no need to GPG sign the package. the -b is build binary
dpkg-buildpackage -us -uc -b -d

#The -us -uc tell it there is no need to GPG sign the package. the -S is build source package
#dpkg-buildpackage -us -uc -S

#dpkg-buildpackage -S

# build source and binary package
#dpkg-buildpackage -us -uc 

#dpkg-buildpackage -b

popd
