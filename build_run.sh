#!/bin/bash

VCPKG_ROOT="/vcpkg"
TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
BUILD_DIR="./build"
REMOTE="prod"
BRANCH="master"

APP_BINARY="$BUILD_DIR/absolute_agony"
PID_FILE="$BUILD_DIR/absolute_agony.pid"
LOG_FILE="/tmp/absolute_agony.log"

cd "$(dirname "$0")" || exit 1

{
    echo "[$(date)] Fetching from remote..."
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

                # Stop running process if any
                if [ -f "$PID_FILE" ]; then
                    PID=$(cat "$PID_FILE")
                    if ps -p "$PID" > /dev/null; then
                        echo "[$(date)] Stopping existing process (PID $PID)..."
                        kill "$PID"
                    fi
                    rm -f "$PID_FILE"
                fi

                echo "[$(date)] Starting new process..."
                export MY_WEBSOCKET_URI="wss://0wl8ctuh90.execute-api.us-east-2.amazonaws.com/production/"
                nohup "$APP_BINARY" >> "$LOG_FILE" 2>&1 &
                echo $! > "$PID_FILE"
                echo "[$(date)] Process started with PID $!"
            else
                echo "[$(date)] Build failed."
            fi
        else
            echo "[$(date)] Git pull failed."
        fi
    else
        echo "[$(date)] Already up-to-date."
        # Restart anyway
        echo "[$(date)] Restarting process..."
        if [ -f "$PID_FILE" ]; then
            PID=$(cat "$PID_FILE")
            if ps -p "$PID" > /dev/null; then
                echo "[$(date)] Stopping existing process (PID $PID)..."
                kill "$PID"
            fi
            rm -f "$PID_FILE"
        fi

        export MY_WEBSOCKET_URI="wss://0wl8ctuh90.execute-api.us-east-2.amazonaws.com/production/"
        nohup "$APP_BINARY" >> "$LOG_FILE" 2>&1 &
        echo $! > "$PID_FILE"
        echo "[$(date)] Process restarted with PID $!"
    fi
} 2>&1 | tee -a /tmp/build_run_output.log
