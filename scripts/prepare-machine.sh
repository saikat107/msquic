#!/bin/bash

# This script installs all necessary dependencies on a Linux machine,
# depending on the provided configuration.
#
# Usage:
#   ./prepare-machine.sh [options]
#
# Options:
#   --tls <openssl|quictls>  The TLS library to use (default: from get-buildconfig.ps1)
#   --force                  Overwrite and force installation of all dependencies
#   --for-container-build    Install dependencies for container builds
#   --for-build              Install dependencies for building
#   --for-test               Install dependencies for testing
#   --install-arm64-toolchain Install ARM64 cross-compilation toolchain
#   --install-clog2text      Install clog2text tool
#   --install-code-coverage  Install code coverage tools (gcovr)
#   --install-duonic         Install DuoNic
#   --use-xdp                Install XDP dependencies
#   --force-xdp-install      Force XDP installation even on non-Ubuntu 24.04
#   --disable-test           Skip initializing the googletest submodule
#   --help                   Display this help message
#
# Examples:
#   ./prepare-machine.sh
#   ./prepare-machine.sh --for-build
#   ./prepare-machine.sh --for-test

set -e  # Exit on error

# Default values
TLS=""
FORCE=0
FOR_CONTAINER_BUILD=0
FOR_BUILD=0
FOR_TEST=0
INSTALL_ARM64_TOOLCHAIN=0
INSTALL_CLOG2TEXT=0
INSTALL_CODE_COVERAGE=0
INSTALL_DUONIC=0
USE_XDP=0
FORCE_XDP_INSTALL=0
DISABLE_TEST=0

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --tls)
            TLS="$2"
            shift 2
            ;;
        --force)
            FORCE=1
            shift
            ;;
        --for-container-build)
            FOR_CONTAINER_BUILD=1
            shift
            ;;
        --for-build)
            FOR_BUILD=1
            shift
            ;;
        --for-test)
            FOR_TEST=1
            shift
            ;;
        --install-arm64-toolchain)
            INSTALL_ARM64_TOOLCHAIN=1
            shift
            ;;
        --install-clog2text)
            INSTALL_CLOG2TEXT=1
            shift
            ;;
        --install-code-coverage)
            INSTALL_CODE_COVERAGE=1
            shift
            ;;
        --install-duonic)
            INSTALL_DUONIC=1
            shift
            ;;
        --use-xdp)
            USE_XDP=1
            shift
            ;;
        --force-xdp-install)
            FORCE_XDP_INSTALL=1
            shift
            ;;
        --disable-test)
            DISABLE_TEST=1
            shift
            ;;
        --help)
            # Extract and display the usage info from the comments
            sed -n '/^# This script/,/^$/p' "$0"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Detect Ubuntu version
IS_UBUNTU_2404=0
if [[ -f /etc/os-release ]]; then
    if grep -q "24.04" /etc/os-release; then
        IS_UBUNTU_2404=1
    fi
fi

# XDP installation warning for non-Ubuntu 24.04 systems
if [[ $USE_XDP -eq 1 && $IS_UBUNTU_2404 -eq 0 && $FORCE_XDP_INSTALL -eq 0 ]]; then
    echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! WARN !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
    echo "Linux XDP installs dependencies from Ubuntu 24.04 packages, which may affect your environment"
    echo "You need to understand the impact of this on your environment before proceeding"
    read -p "Type 'YES' to proceed: " user_input
    if [[ "$user_input" != "YES" ]]; then
        echo "User did not type YES. Exiting script."
        exit 1
    fi
fi

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
ARTIFACTS_PATH="$ROOT_DIR/artifacts"

# Create artifacts directory if it doesn't exist
mkdir -p "$ARTIFACTS_PATH"

# When no args are passed, assume we want to build and test everything locally
if [[ $FOR_CONTAINER_BUILD -eq 0 && $FOR_BUILD -eq 0 && $FOR_TEST -eq 0 ]]; then
    echo "No arguments passed, defaulting to --for-build and --for-test"
    FOR_BUILD=1
    FOR_TEST=1
fi

# Install DuoNic for Linux
install_duonic() {
    echo "Creating DuoNic endpoints"
    DUONIC_SCRIPT="$SCRIPT_DIR/duonic.sh"
    if [[ -f "$DUONIC_SCRIPT" ]]; then
        sudo bash "$DUONIC_SCRIPT" install
    else
        echo "Warning: duonic.sh not found at $DUONIC_SCRIPT"
    fi
}

# Install clog2text tool
install_clog2text() {
    echo "Initializing clog submodule"
    cd "$ROOT_DIR"
    git submodule init submodules/clog
    git submodule update
    
    dotnet build "$ROOT_DIR/submodules/clog"
    NUGET_PATH="$ROOT_DIR/submodules/clog/src/nupkg"
    
    if [[ -f "$NUGET_PATH/Microsoft.Logging.CLOG2Text.Lttng.0.0.1.nupkg" ]]; then
        echo "Installing: Microsoft.Logging.CLOG2Text.Lttng"
        dotnet tool update --global --add-source "$NUGET_PATH" Microsoft.Logging.CLOG2Text.Lttng || {
            echo "Microsoft.Logging.CLOG2Text.Lttng could not be installed. Parsing lttng logs will fail"
        }
    else
        echo "Microsoft.Logging.CLOG2Text.Lttng not found. Parsing lttng logs will fail"
    fi
}

# Install code coverage tools
install_code_coverage() {
    # Check if gcovr is already installed
    if command -v gcovr &> /dev/null; then
        echo "gcovr is already installed"
        return
    fi
    
    # Check if pip is already installed, and if not, install it
    if command -v pip &> /dev/null; then
        echo "pip is already installed"
    else
        echo "Installing pip"
        sudo apt-get update -y
        sudo apt-get install -y python3-pip
    fi
    
    pip install gcovr==8.6
}

# Initialize git submodules
initialize_submodules() {
    echo "Initializing clog submodule"
    cd "$ROOT_DIR"
    git submodule init submodules/clog
    
    # Get TLS configuration if not set
    if [[ -z "$TLS" ]]; then
        # Try to get TLS from get-buildconfig.ps1 if PowerShell is available
        if command -v pwsh &> /dev/null; then
            TLS=$(pwsh "$SCRIPT_DIR/get-buildconfig.ps1" -Tls "$TLS" 2>/dev/null | sed -n 's/.*Tls[[:space:]]*:[[:space:]]*\([a-zA-Z]*\).*/\1/p' || echo "openssl")
        else
            TLS="openssl"
        fi
    fi
    
    if [[ "$TLS" == "quictls" ]]; then
        echo "Initializing quictls submodule"
        git submodule init submodules/quictls
    fi
    
    if [[ "$TLS" == "openssl" ]]; then
        echo "Initializing openssl submodule"
        git submodule init submodules/openssl
    fi
    
    if [[ $DISABLE_TEST -eq 0 ]]; then
        echo "Initializing googletest submodule"
        git submodule init submodules/googletest
    fi
    
    git submodule update --jobs=8
}

# Install build dependencies
install_build_dependencies() {
    echo "Installing build dependencies..."
    
    sudo apt-add-repository ppa:lttng/stable-2.13 -y
    sudo apt-get update -y
    sudo apt-get install -y cmake
    sudo apt-get install -y build-essential
    sudo apt-get install -y liblttng-ust-dev
    
    # Try to install babeltrace2 first, then fallback to babeltrace
    if ! sudo apt-get install -y babeltrace2; then
        sudo apt-get install -y babeltrace
    fi
    
    sudo apt-get install -y libssl-dev
    sudo apt-get install -y libnuma-dev
    sudo apt-get install -y liburing-dev
    
    if [[ $INSTALL_ARM64_TOOLCHAIN -eq 1 ]]; then
        sudo apt-get install -y gcc-aarch64-linux-gnu
        sudo apt-get install -y binutils-aarch64-linux-gnu
        sudo apt-get install -y g++-aarch64-linux-gnu
    fi
    
    # Only used for the codecheck CI run:
    sudo apt-get install -y cppcheck clang-tidy
    
    # Used for packaging
    sudo apt-get install -y ruby ruby-dev rpm
    sudo gem install public_suffix -v 4.0.7
    sudo gem install fpm
    
    # XDP dependencies
    if [[ $USE_XDP -eq 1 ]]; then
        sudo apt-get -y install --no-install-recommends libc6-dev-i386 # for building xdp programs
        if [[ $IS_UBUNTU_2404 -eq 0 ]]; then
            sudo apt-add-repository "deb http://mirrors.kernel.org/ubuntu noble main" -y
            sudo apt-get update -y
        fi
        sudo apt-get -y install libxdp-dev libbpf-dev
        sudo apt-get -y install libnl-3-dev libnl-genl-3-dev libnl-route-3-dev zlib1g-dev zlib1g pkg-config m4 clang libpcap-dev libelf-dev
    fi
}

# Install test dependencies
install_test_dependencies() {
    echo "Installing test dependencies..."
    
    sudo apt-add-repository ppa:lttng/stable-2.13 -y
    sudo apt-get update -y
    sudo apt-get install -y lttng-tools
    sudo apt-get install -y liblttng-ust-dev
    sudo apt-get install -y gdb
    sudo apt-get install -y liburing2
    
    if [[ $USE_XDP -eq 1 ]]; then
        if [[ $IS_UBUNTU_2404 -eq 0 ]]; then
            sudo apt-add-repository "deb http://mirrors.kernel.org/ubuntu noble main" -y
            sudo apt-get update -y
        fi
        sudo apt-get install -y libxdp1 libbpf1
        sudo apt-get install -y libnl-3-200 libnl-route-3-200 libnl-genl-3-200
        sudo apt-get install -y iproute2 iptables
        install_duonic
    fi
    
    # Enable core dumps for the system
    echo "Setting core dump size limit"
    sudo sh -c "echo 'root soft core unlimited' >> /etc/security/limits.conf"
    sudo sh -c "echo 'root hard core unlimited' >> /etc/security/limits.conf"
    sudo sh -c "echo '* soft core unlimited' >> /etc/security/limits.conf"
    sudo sh -c "echo '* hard core unlimited' >> /etc/security/limits.conf"
    
    # Increase the number of file descriptors
    sudo sh -c "echo 'root soft nofile 1048576' >> /etc/security/limits.conf"
    sudo sh -c "echo 'root hard nofile 1048576' >> /etc/security/limits.conf"
    sudo sh -c "echo '* soft nofile 1048576' >> /etc/security/limits.conf"
    sudo sh -c "echo '* hard nofile 1048576' >> /etc/security/limits.conf"
    
    # Set the core dump pattern
    echo "Setting core dump pattern"
    sudo sh -c "echo -n '%e.%p.%t.core' > /proc/sys/kernel/core_pattern"
}

# Main execution
echo "Starting MsQuic Linux machine preparation..."

if [[ $FOR_BUILD -eq 1 || $FOR_CONTAINER_BUILD -eq 1 ]]; then
    initialize_submodules
fi

if [[ $INSTALL_CLOG2TEXT -eq 1 ]]; then
    sudo apt-get update -y
    sudo apt-get install -y dotnet-runtime-8.0
    install_clog2text
fi

if [[ $FOR_BUILD -eq 1 ]]; then
    install_build_dependencies
fi

if [[ $FOR_TEST -eq 1 ]]; then
    install_test_dependencies
fi

if [[ $INSTALL_DUONIC -eq 1 ]]; then
    install_duonic
fi

if [[ $INSTALL_CODE_COVERAGE -eq 1 ]]; then
    install_code_coverage
fi

echo "Machine preparation completed successfully!"
