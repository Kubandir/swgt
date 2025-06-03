#ifndef CONFIG_H
#define CONFIG_H

#define BUTTON_SIZE 60
#define BUTTON_MARGIN 8
#define WIDGET_PADDING 8
#define ICON_TEXT_SPACING 8
#define BORDER_WIDTH 2

#define WIDGET_WIDTH (WIDGET_PADDING * 2 + BUTTON_SIZE)
#define WIDGET_HEIGHT (WIDGET_PADDING * 2 + (BUTTON_COUNT * BUTTON_SIZE) + ((BUTTON_COUNT - 1) * BUTTON_MARGIN))

#define MAX_ANIMATION_FRAMES 15
#define IDLE_SLEEP_MS 50
#define ANIMATION_SLEEP_MS 16
#define HOVER_ZONE_WIDTH 10
#define HOVER_ZONE_HEIGHT 100

#define BG_COLOR        "#16161e"
#define TEXT_COLOR      "#fbf9fe"
#define WINDOW_BORDER_COLOR "#9c98d5"
#define BORDER_COLOR    "#5a4b8b"
#define BUTTON_BG_COLOR "#202028"
#define ACTIVE_BG_COLOR "#2a2a3a"
#define ACTIVE_TEXT_COLOR "#c6a0f6"
#define ACTIVE_BORDER_COLOR "#c6a0f6"
#define PRESSED_BG_COLOR "#1a1a22"
#define ICON_COLOR      "#fbf9fe"
#define ACTIVE_ICON_COLOR "#c6a0f6"

#define ICON_FONT_NAME  "Symbols Nerd Font"
#define ICON_FONT_SIZE  15
#define TEXT_FONT_NAME  "BigBlueTermPlus Nerd Font Mono:weight=Regular:antialias=true:hinting=true"
#define TEXT_FONT_SIZE  10

#define WINDOW_NAME "SWGT"
#define WINDOW_CLASS_NAME "swgt"
#define WINDOW_CLASS_CLASS "SWGT"

#define BUTTON_COUNT 5
/* icon, text, command_on, command_off, click_only */
#define BUTTON_CONFIGS { \
    {"\uf0f3", "Dnd", "pkill -SIGUSR1 dunst", "pkill -SIGUSR2 dunst", 0}, \
    {"\uf185", "Night", "redshift -O 2000", "redshift -x", 0}, \
    {"\uf0ae", "Work", "~/code/swgt/scripts/productivity.sh start", "~/code/swgt/scripts/productivity.sh stop", 0}, \
    {"\uf072", "Air", "rfkill block all; bluetoothctl power off", "rfkill unblock all; sleep 1; bluetoothctl power on", 0}, \
    {"\uf011", "Power", "~/code/swgt/scripts/power.sh", "", 1} \
}

#endif
