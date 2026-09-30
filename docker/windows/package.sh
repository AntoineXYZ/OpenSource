#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 release|debug|relwithdebinfo" >&2
    exit 2
fi

config=$1
case "$config" in
    release|debug|relwithdebinfo) ;;
    *) echo "error: unsupported build configuration: $config" >&2; exit 2 ;;
esac

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
source "$repo_root/docker/support/staging.sh"
daemon_repo_root=$(bash "$repo_root/docker/support/resolve-project-mount.sh" "$repo_root")
image=${HAZERON_MINGW_IMAGE:-hazeron-mingw-qt5}
local_output="$repo_root/build/windows-$config"
daemon_output="$daemon_repo_root/build/windows-$config"
local_packages="$repo_root/build/packages"
daemon_packages="$daemon_repo_root/build/packages"
mkdir -p "$local_output" "$local_packages"

if ! docker image inspect "$image" >/dev/null 2>&1; then
    echo "error: Windows toolchain image does not exist: $image" >&2
    echo "Run make win before packaging." >&2
    exit 1
fi

create_staging_directory "$local_output" package-stage
daemon_stage="$daemon_output/${staging_dir##*/}"

docker run --rm \
    --user "$(id -u):$(id -g)" \
    --env HOME=/tmp \
    --mount "type=bind,source=$daemon_repo_root,target=/src,readonly" \
    --mount "type=bind,source=$daemon_output,target=/output,readonly" \
    --mount "type=bind,source=$daemon_stage,target=/stage" \
    --mount "type=bind,source=$daemon_packages,target=/packages" \
    "$image" \
    /bin/bash -ec '
        stage=/stage
        packages=/packages
        game_name=soh-win64
        launcher_name=soh-launcher-win64
        game="$stage/$game_name"
        launcher="$stage/$launcher_name"
        qt_plugins="/opt/mxe/usr/${MXE_TARGET}/qt5/plugins"
        target_prefix="/opt/mxe/usr/${MXE_TARGET}"

        require_file() {
            if [[ ! -f "$1" ]]; then
                echo "error: required package input is missing: $1" >&2
                exit 1
            fi
        }
        for file in AuSceneClient.exe AuChatClient.exe AuMailClient.exe AuLauncher.exe GD sfx.har; do
            require_file "/output/bin/$file"
        done
        for directory in tex Blueprints/Buildings Blueprints/Spacecraft; do
            if [[ ! -d "/output/bin/$directory" ]]; then
                echo "error: required client runtime directory is missing: /output/bin/$directory" >&2
                exit 1
            fi
        done

        mkdir -p "$game" "$launcher" "$packages"

        install -m 755 /output/bin/AuSceneClient.exe "$game/Shores of Hazeron.exe"
        install -m 755 /output/bin/AuChatClient.exe "$game/Hazeron Commander.exe"
        install -m 755 /output/bin/AuMailClient.exe "$game/Hazeron Mail.exe"
        install -m 644 /output/bin/GD "$game/GD"
        install -m 644 /output/bin/sfx.har "$game/sfx.har"
        cp -a /output/bin/tex "$game/tex"
        cp -a /output/bin/Blueprints "$game/Blueprints"

        resource_libraries=(
            libAuAtomRes.dll
            libAuBodyPartsRes.dll
            libAuClientMailRes.dll
            libAuClientRes.dll
            libAuModelRes.dll
            libAuStoryRes.dll
            libAuTexture.dll
        )
        for library in "${resource_libraries[@]}"; do
            require_file "/output/bin/$library"
            cp "/output/bin/$library" "$game/$library"
        done

        shopt -s nullglob

        install -m 755 /output/bin/AuLauncher.exe "$launcher/Hazeron Launcher.exe"
        printf "%s\n" "$game_name" > "$launcher/data.bin"

        copy_plugins() {
            local destination=$1
            shift
            local plugin_group files
            for plugin_group in "$@"; do
                mkdir -p "$destination/$plugin_group"
                files=("$qt_plugins/$plugin_group/"*.dll)
                if (( ${#files[@]} > 0 )); then
                    cp "${files[@]}" "$destination/$plugin_group/"
                fi
            done
        }
        copy_plugins "$game" platforms imageformats styles bearer iconengines sqldrivers
        copy_plugins "$launcher" platforms imageformats styles bearer iconengines

        python3 /opt/mxe/tools/copydlldeps.py \
            "$game" \
            -C "$game" \
               "$game/platforms" \
               "$game/imageformats" \
               "$game/styles" \
               "$game/bearer" \
               "$game/iconengines" \
               "$game/sqldrivers" \
            -L /output/bin \
               "$target_prefix/bin" \
               "$target_prefix/qt5/bin"
        # Use the system OpenGL DLL consistently with the Qt WGL context.
        rm -f "$game/opengl32.dll" "$game/libgallium_wgl.dll"
        python3 /opt/mxe/tools/copydlldeps.py \
            "$launcher" \
            -C "$launcher" \
               "$launcher/platforms" \
               "$launcher/imageformats" \
               "$launcher/styles" \
               "$launcher/bearer" \
               "$launcher/iconengines" \
            -L /output/bin \
               "$target_prefix/bin" \
               "$target_prefix/qt5/bin"

        write_manifest() {
            local root=$1
            (
                cd "$root"
                find . -type f ! -name md5sums -print0 \
                    | sort -z \
                    | xargs -0 md5sum > md5sums
            )
        }
        write_manifest "$game"
        write_manifest "$launcher"

        rm -f "$packages/$game_name.zip" "$packages/$launcher_name.zip"
        (cd "$stage" && cmake -E tar cf "$packages/$game_name.zip" --format=zip "$game_name")
        (cd "$stage" && cmake -E tar cf "$packages/$launcher_name.zip" --format=zip "$launcher_name")
        (
            cd "$packages"
            sha256sum \
                "$game_name.zip" \
                "$launcher_name.zip" \
                > windows-x64.sha256
        )
    '

printf 'Windows game package: %s\n' "$repo_root/build/packages/soh-win64.zip"
printf 'Windows launcher package: %s\n' "$repo_root/build/packages/soh-launcher-win64.zip"
