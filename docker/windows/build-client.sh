#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
image=${HAZERON_MINGW_IMAGE:-hazeron-mingw-qt5}
config=${HAZERON_BUILD_CONFIG:-release}
target=${AU_TARGET:-client}

if [[ "$target" != client && "$target" != clients ]]; then
    echo "error: unsupported Windows client target: $target" >&2
    exit 2
fi

case "$config" in
    debug)
        cmake_build_type=Debug
        ;;
    release)
        cmake_build_type=Release
        ;;
    relwithdebinfo)
        cmake_build_type=RelWithDebInfo
        ;;
    *)
        echo "error: unsupported build configuration: $config" >&2
        exit 2
        ;;
esac

daemon_repo_root=$(bash "$repo_root/docker/support/resolve-project-mount.sh" "$repo_root")
cache_volume=${HAZERON_WINDOWS_CACHE_VOLUME:-hazeron-windows-${config}-cache}
local_output_dir="$repo_root/build/windows-${config}"
daemon_output_dir="$daemon_repo_root/build/windows-${config}"
local_editor_root="$repo_root/build/editor"
daemon_editor_root="$daemon_repo_root/build/editor"
editor_workspace_root=$repo_root
if [[ -x /usr/bin/wslpath ]] && grep -qi microsoft /proc/sys/kernel/osrelease; then
    editor_workspace_root=$(wslpath -m "$repo_root")
fi
mkdir -p "$local_output_dir" "$local_editor_root"

cd "$repo_root"

toolchain_healthy() {
    docker run --rm "$image" /bin/bash -ec '
        for file in \
            /opt/mxe/usr/x86_64-pc-linux-gnu/bin/ninja \
            "/opt/mxe/usr/${MXE_TARGET}/share/cmake/mxe-conf.cmake" \
            "/opt/mxe/usr/bin/${MXE_TARGET}-g++"; do
            if [[ ! -s "$file" ]]; then
                echo "error: missing or empty toolchain file: $file" >&2
                exit 1
            fi
        done
        /opt/mxe/usr/x86_64-pc-linux-gnu/bin/ninja --version >/dev/null
    '
}

if ! docker image inspect "$image" >/dev/null 2>&1; then
    docker build \
        --tag "$image" \
        --file docker/windows/Dockerfile \
        docker/windows
fi

if ! toolchain_healthy; then
    echo "Windows toolchain image is damaged; rebuilding it without cached layers." >&2
    docker build \
        --no-cache \
        --tag "$image" \
        --file docker/windows/Dockerfile \
        docker/windows
fi

if ! toolchain_healthy; then
    echo "error: Windows toolchain image failed validation after build" >&2
    exit 1
fi

image_id=$(docker image inspect "$image" --format '{{.Id}}')
docker volume create "$cache_volume" >/dev/null
docker run --rm \
    --mount "type=volume,source=$cache_volume,target=/build" \
    "$image" chown "$(id -u):$(id -g)" /build

# MXE uses ccache. Keep it writable by the build user, not inside the image.
docker run --rm \
    --user "$(id -u):$(id -g)" \
    --env HOME=/tmp \
    --env CCACHE_DIR=/build/ccache \
    --env "CMAKE_BUILD_TYPE=$cmake_build_type" \
    --env "AU_IMAGE_ID=$image_id" \
    --env "AU_TARGET=$target" \
    --env "AU_WORKSPACE_ROOT=$editor_workspace_root" \
    --mount "type=bind,source=$daemon_repo_root,target=/src,readonly" \
    --mount "type=volume,source=$cache_volume,target=/build" \
    --mount "type=bind,source=$daemon_output_dir,target=/output" \
    --mount "type=bind,source=$daemon_editor_root,target=/editor" \
    "$image" \
    /bin/bash -ec '
        package_dir=/output/bin
        qt_plugins="/opt/mxe/usr/${MXE_TARGET}/qt5/plugins"
        target_prefix="/opt/mxe/usr/${MXE_TARGET}"

        "${MXE_TARGET}-cmake" \
            -S /src \
            -B /build \
            -G Ninja \
            -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
            -DAU_OUTPUT_ROOT=/output
        cmake --build /build \
            --target "$AU_TARGET" \
            --parallel "$(nproc)"

        find "$package_dir" -maxdepth 1 -type f \
            -iname "*.dll" ! -iname "libAu*.dll" -delete
        rm -rf \
            "$package_dir/platforms" \
            "$package_dir/imageformats" \
            "$package_dir/styles" \
            "$package_dir/bearer"
        mkdir -p \
            "$package_dir/platforms" \
            "$package_dir/imageformats" \
            "$package_dir/styles" \
            "$package_dir/bearer"
        cp "$qt_plugins/platforms/qwindows.dll" "$package_dir/platforms/"
        cp "$qt_plugins/imageformats/"*.dll "$package_dir/imageformats/"
        cp "$qt_plugins/styles/"*.dll "$package_dir/styles/"
        cp "$qt_plugins/bearer/"*.dll "$package_dir/bearer/"

        python3 /opt/mxe/tools/copydlldeps.py \
            "$package_dir" \
            -C "$package_dir" \
               "$package_dir/platforms" \
               "$package_dir/imageformats" \
               "$package_dir/styles" \
               "$package_dir/bearer" \
            -L "$target_prefix/bin" \
               "$target_prefix/qt5/bin"

        # MXE can supply a Mesa opengl32.dll here. Qt uses the system WGL
        # implementation, so loading both OpenGL DLLs breaks widget painting.
        # Let the Windows loader resolve opengl32.dll from System32 for the game.
        rm -f "$package_dir/opengl32.dll" "$package_dir/libgallium_wgl.dll"

        python3 /src/docker/support/export-intellisense.py \
            --compile-commands /build/compile_commands.json \
            --compiler "${MXE_TARGET}-g++" \
            --source-root /src \
            --build-root /build \
            --output-root /editor/windows \
            --workspace-root "$AU_WORKSPACE_ROOT" \
            --image-id "$AU_IMAGE_ID"
    '

printf 'Windows artifacts: %s/build/windows-%s\n' "$repo_root" "$config"
printf 'Windows cache volume: %s\n' "$cache_volume"
