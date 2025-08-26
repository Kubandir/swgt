#!/bin/bash
set -euo pipefail

APP_NAME="Timer"
PID_FILE="/tmp/swgt_timer.pid"
FIFO="/tmp/swgt_timer.fifo"

notify() {
    # Prefer dunstify if present, fallback to notify-send
    if command -v dunstify >/dev/null 2>&1; then
        dunstify "$APP_NAME" "$1"
    else
        notify-send "$APP_NAME" "$1"
    fi
}

is_running() {
    [[ -f "$PID_FILE" ]] && kill-0 "$(cat "$PID_FILE")" 2>/dev/null
}

send_add_minute() {
    # Adds 60 seconds to the running timer
    if [[ -p "$FIFO" ]]; then
        printf 'ADD 60\n' > "$FIFO" || true
    else
        notify "Timer is running but control channel is missing."
    fi
}

start_daemon() {
    echo $$ > "$PID_FILE"
    trap 'rm -f "$PID_FILE" "$FIFO"; exit 0' INT TERM EXIT

    # Ensure FIFO exists (recreate if needed)
    rm -f "$FIFO"
    mkfifo -m 600 "$FIFO"

    # Open FIFO RDWR so writers never block on open
    exec 3<>"$FIFO"

    seconds_left=60
    notify "Started for 1 minute."
    last_notified_minutes=-1

    while :; do
        # Drain any pending commands without blocking
        while IFS= read -r -t 0 -u 3 line; do
            case "$line" in
                ADD\ *)
                    add="${line#ADD }"
                    [[ "$add" =~ ^[0-9]+$ ]] || add=0
                    seconds_left=$((seconds_left + add))
                    mins=$(( (seconds_left + 59) / 60 ))
                    notify "Added $((add/60)) minute. $mins minute(s) remaining."
                    ;;
                RESET|STOP)
                    seconds_left=0
                    ;;
            esac
        done

        if (( seconds_left <= 0 )); then
            notify "Timer finished."
            break
        fi

        sleep 1
        seconds_left=$((seconds_left - 1))

        # Notify each minute boundary, but never for 0 minutes remaining
        mins=$(( (seconds_left + 59) / 60 ))
        if (( seconds_left % 60 == 0 )) && (( mins != last_notified_minutes )) && (( mins > 0 )); then
            notify "$mins minute(s) remaining..."
            last_notified_minutes=$mins
        fi
    done
}

# Main entry
if is_running; then
    # Already running: just add one minute
    send_add_minute
    exit 0
fi

# Clean stale state if any and start fresh 1-minute timer
rm -f "$PID_FILE"
start_daemon
