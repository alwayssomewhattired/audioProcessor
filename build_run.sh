#!/bin/bash

exec > >(tee >(logger -t build_run.sh)) 2> >(tee >(logger -t build_run.sh >&2))
set -x

VCPKG_ROOT="/vcpkg"
TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
BUILD_DIR="./build"
REMOTE="prod"
BRANCH="master"
APP_BINARY="$BUILD_DIR/absolute_agony"

cd "$(dirname "$0")" || exit 1

git fetch "$REMOTE" "$BRANCH"

LOCAL_HASH=$(git rev-parse "$BRANCH")
REMOTE_HASH=$(git rev-parse "$REMOTE/$BRANCH")

echo "[$(date)] Local hash: $LOCAL_HASH"
echo "[$(date)] Remote hash: $REMOTE_HASH"

if [ "$LOCAL_HASH" != "$REMOTE_HASH" ]; then
    echo "[$(date)] New commit detected. Pulling..."
    if git pull --ff-only "$REMOTE" "$BRANCH"; then
        echo "[$(date)] Pull complete. Building..."

        mkdir -p "$BUILD_DIR"

        if cmake -B "$BUILD_DIR" -S . \
            -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
            -DVCPKG_TARGET_TRIPLET=x64-linux \
            -DCMAKE_PREFIX_PATH="/vcpkg/installed/x64-linux/share" \
            && cmake --build "$BUILD_DIR"; then

            echo "[$(date)] Build complete. Running app..."

        else
            echo "[$(date)] Build failed. Skipping run."
            exit 1
        fi
    else
        echo "[$(date)] Git pull failed. Skipping build/run."
        exit 1
    fi
else
    echo "[$(date)] No new commits. Already up to date."
fi

cd "$BUILD_DIR"
export MY_WEBSOCKET_URI="wss://0wl8ctuh90.execute-api.us-east-2.amazonaws.com/production/"
"./absolute_agony"
