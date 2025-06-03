#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <X11/Xft/Xft.h>
#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#include "config.h"

typedef struct {
    char icon[8];
    char text[32];
    char toggle_command[256];
    char untoggle_command[256];
    int is_active;
    int is_pressed;
    int click_only;
} Button;

typedef struct {
    Display *display;
    Window window;
    Window root_window;
    GC gc;
    XftDraw *xft_draw;
    XftFont *icon_font, *text_font;
    XftColor xft_text_color, xft_active_text_color, xft_icon_color, xft_active_icon_color;
    Colormap colormap;
    Visual *visual;
    int has_compositor;
    
    XColor bg_color, text_color, window_border_color, border_color, button_bg_color;
    XColor active_bg_color, active_text_color, active_border_color;
    XColor pressed_bg_color;
    
    int screen_width, screen_height;
    int current_x, target_x, hidden_x;
    int is_visible, is_closing, is_animating, mouse_in_zone;
    int needs_redraw;
    int frame_count;
    
    Button buttons[BUTTON_COUNT];
} Widget;

// Function declarations
void init_widget(Widget *widget);
void animate_widget(Widget *widget);
void start_close_animation(Widget *widget);
void start_show_animation(Widget *widget);
void draw_widget(Widget *widget);
void draw_button(Widget *widget, int index);
void cleanup_widget(Widget *widget);
int check_mouse_in_hover_zone(Widget *widget);
int check_mouse_over_widget(Widget *widget);
void setup_colors(Widget *widget);
void setup_fonts(Widget *widget);
void execute_command(const char *command);
void init_buttons(Widget *widget);
int get_button_at_position(Widget *widget, int x, int y);
void toggle_button(Widget *widget, int button_index);
void draw_text_centered_xft(Widget *widget, const char *text, int x, int y, int width, XftFont *font, XftColor *color);
int check_compositor(Display *display);
void set_window_opacity(Widget *widget, double opacity);

void execute_command(const char *command) {
    if (!command[0]) return;
    
    if (fork() == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", command, NULL);
        exit(1);
    }
}

void init_buttons(Widget *widget) {
    const struct {
        char icon[8];
        char text[32];
        char toggle_command[256];
        char untoggle_command[256];
        int click_only;
    } button_configs[] = BUTTON_CONFIGS;
    
    for (int i = 0; i < BUTTON_COUNT; i++) {
        strcpy(widget->buttons[i].icon, button_configs[i].icon);
        strcpy(widget->buttons[i].text, button_configs[i].text);
        strcpy(widget->buttons[i].toggle_command, button_configs[i].toggle_command);
        strcpy(widget->buttons[i].untoggle_command, button_configs[i].untoggle_command);
        widget->buttons[i].click_only = button_configs[i].click_only;
        widget->buttons[i].is_active = 0;
        widget->buttons[i].is_pressed = 0;
    }
}

XColor parse_color(Widget *widget, const char *color_str) {
    XColor color;
    if (!XParseColor(widget->display, widget->colormap, color_str, &color) ||
        !XAllocColor(widget->display, widget->colormap, &color)) {
        color.red = color.green = color.blue = 65535;
        XAllocColor(widget->display, widget->colormap, &color);
    }
    return color;
}

void setup_colors(Widget *widget) {
    int screen = DefaultScreen(widget->display);
    widget->colormap = DefaultColormap(widget->display, screen);
    widget->visual = DefaultVisual(widget->display, screen);
    
    widget->bg_color = parse_color(widget, BG_COLOR);
    widget->text_color = parse_color(widget, TEXT_COLOR);
    widget->window_border_color = parse_color(widget, WINDOW_BORDER_COLOR);
    widget->border_color = parse_color(widget, BORDER_COLOR);
    widget->button_bg_color = parse_color(widget, BUTTON_BG_COLOR);
    widget->active_bg_color = parse_color(widget, ACTIVE_BG_COLOR);
    widget->active_text_color = parse_color(widget, ACTIVE_TEXT_COLOR);
    widget->active_border_color = parse_color(widget, ACTIVE_BORDER_COLOR);
    widget->pressed_bg_color = parse_color(widget, PRESSED_BG_COLOR);
    
    XftColorAllocName(widget->display, widget->visual, widget->colormap, TEXT_COLOR, &widget->xft_text_color);
    XftColorAllocName(widget->display, widget->visual, widget->colormap, ACTIVE_TEXT_COLOR, &widget->xft_active_text_color);
    XftColorAllocName(widget->display, widget->visual, widget->colormap, ICON_COLOR, &widget->xft_icon_color);
    XftColorAllocName(widget->display, widget->visual, widget->colormap, ACTIVE_ICON_COLOR, &widget->xft_active_icon_color);
}

void setup_fonts(Widget *widget) {
    char icon_pattern[256], text_pattern[256];
    
    snprintf(icon_pattern, sizeof(icon_pattern), "%s:size=%d", ICON_FONT_NAME, ICON_FONT_SIZE);
    snprintf(text_pattern, sizeof(text_pattern), "%s:size=%d", TEXT_FONT_NAME, TEXT_FONT_SIZE);
    
    widget->icon_font = XftFontOpenName(widget->display, DefaultScreen(widget->display), icon_pattern);
    if (!widget->icon_font)
        widget->icon_font = XftFontOpenName(widget->display, DefaultScreen(widget->display), "monospace:size=20");
    
    widget->text_font = XftFontOpenName(widget->display, DefaultScreen(widget->display), text_pattern);
    if (!widget->text_font)
        widget->text_font = XftFontOpenName(widget->display, DefaultScreen(widget->display), "monospace:size=10");
    
    if (!widget->icon_font || !widget->text_font)
        exit(1);
}

void init_widget(Widget *widget) {
    widget->display = XOpenDisplay(NULL);
    if (!widget->display)
        exit(1);
    
    int screen = DefaultScreen(widget->display);
    widget->root_window = RootWindow(widget->display, screen);
    widget->screen_width = DisplayWidth(widget->display, screen);
    widget->screen_height = DisplayHeight(widget->display, screen);
    
    // Check for compositor
    widget->has_compositor = check_compositor(widget->display);
    
    setup_colors(widget);
    setup_fonts(widget);
    init_buttons(widget);
    
    widget->hidden_x = widget->screen_width;
    widget->target_x = widget->screen_width - WIDGET_WIDTH;
    widget->current_x = widget->hidden_x;
    widget->is_visible = 0;
    widget->is_closing = 0;
    widget->is_animating = 0;
    widget->mouse_in_zone = 0;
    widget->needs_redraw = 1;
    widget->frame_count = 0;
    
    widget->window = XCreateSimpleWindow(
        widget->display, widget->root_window,
        widget->current_x, (widget->screen_height - WIDGET_HEIGHT) / 2,
        WIDGET_WIDTH, WIDGET_HEIGHT, BORDER_WIDTH,
        widget->window_border_color.pixel, widget->bg_color.pixel
    );
    
    XStoreName(widget->display, widget->window, WINDOW_NAME);
    XClassHint class_hint = {WINDOW_CLASS_NAME, WINDOW_CLASS_CLASS};
    XSetClassHint(widget->display, widget->window, &class_hint);
    
    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    XChangeWindowAttributes(widget->display, widget->window, CWOverrideRedirect, &attrs);
    
    XGCValues gc_values;
    gc_values.foreground = widget->text_color.pixel;
    gc_values.background = widget->bg_color.pixel;
    widget->gc = XCreateGC(widget->display, widget->window, GCForeground | GCBackground, &gc_values);
    
    widget->xft_draw = XftDrawCreate(widget->display, widget->window, widget->visual, widget->colormap);
    
    XSelectInput(widget->display, widget->window, 
                ExposureMask | ButtonPressMask | ButtonReleaseMask | StructureNotifyMask);
    
    XMapWindow(widget->display, widget->window);
    
    // Set window opacity after mapping if compositor is available
    if (widget->has_compositor) {
        set_window_opacity(widget, WINDOW_OPACITY);
    }
    
    XFlush(widget->display);
}

int check_mouse_in_hover_zone(Widget *widget) {
    Window root_return, child_return;
    int root_x, root_y, win_x, win_y;
    unsigned int mask_return;
    
    if (!XQueryPointer(widget->display, widget->root_window,
                      &root_return, &child_return,
                      &root_x, &root_y, &win_x, &win_y, &mask_return))
        return 0;
    
    int widget_y = (widget->screen_height - WIDGET_HEIGHT) / 2;
    return (root_x >= widget->screen_width - HOVER_ZONE_WIDTH &&
            root_y >= widget_y - HOVER_ZONE_HEIGHT && 
            root_y <= widget_y + WIDGET_HEIGHT + HOVER_ZONE_HEIGHT);
}

int check_mouse_over_widget(Widget *widget) {
    Window root_return, child_return;
    int root_x, root_y, win_x, win_y;
    unsigned int mask_return;
    
    if (!XQueryPointer(widget->display, widget->root_window,
                      &root_return, &child_return,
                      &root_x, &root_y, &win_x, &win_y, &mask_return))
        return 0;
    
    int widget_y = (widget->screen_height - WIDGET_HEIGHT) / 2;
    return (root_x >= widget->current_x && 
            root_x <= widget->current_x + WIDGET_WIDTH &&
            root_y >= widget_y && 
            root_y <= widget_y + WIDGET_HEIGHT);
}

void animate_widget(Widget *widget) {
    if (widget->frame_count >= MAX_ANIMATION_FRAMES) {
        widget->current_x = widget->is_closing ? widget->hidden_x : widget->target_x;
        widget->is_visible = !widget->is_closing;
        widget->is_closing = 0;
        widget->is_animating = 0;
        widget->frame_count = 0;
        
        XMoveWindow(widget->display, widget->window, widget->current_x,
                   (widget->screen_height - WIDGET_HEIGHT) / 2);
        widget->needs_redraw = 1;
        return;
    }
    
    float frame_progress = (float)widget->frame_count / MAX_ANIMATION_FRAMES;
    float ease_out = 1.0f - (1.0f - frame_progress) * (1.0f - frame_progress) * (1.0f - frame_progress);
    
    if (widget->is_closing) {
        widget->current_x = widget->target_x + (int)((widget->hidden_x - widget->target_x) * ease_out);
    } else {
        widget->current_x = widget->hidden_x - (int)((widget->hidden_x - widget->target_x) * ease_out);
    }
    
    XMoveWindow(widget->display, widget->window, widget->current_x,
               (widget->screen_height - WIDGET_HEIGHT) / 2);
    
    widget->needs_redraw = 1;
    widget->frame_count++;
}

void start_close_animation(Widget *widget) {
    if (widget->is_visible && !widget->is_closing && !widget->is_animating) {
        widget->is_closing = 1;
        widget->is_animating = 1;
        widget->frame_count = 0;
    }
}

void start_show_animation(Widget *widget) {
    if (!widget->is_visible && !widget->is_closing && !widget->is_animating) {
        widget->mouse_in_zone = 1;
        widget->is_animating = 1;
        widget->frame_count = 0;
    }
}

void draw_text_centered_xft(Widget *widget, const char *text, int x, int y, int width, XftFont *font, XftColor *color) {
    if (!text || !text[0]) return;
    
    XGlyphInfo extents;
    XftTextExtentsUtf8(widget->display, font, (FcChar8*)text, strlen(text), &extents);
    
    int text_x = x + (width - extents.width) / 2;
    XftDrawStringUtf8(widget->xft_draw, color, font, text_x, y, (FcChar8*)text, strlen(text));
}

void draw_button(Widget *widget, int index) {
    Button *button = &widget->buttons[index];
    
    int button_x = WIDGET_PADDING;
    int button_y = WIDGET_PADDING + index * (BUTTON_SIZE + BUTTON_MARGIN);
    
    XColor *bg_color, *border_color;
    XftColor *text_color, *icon_color;
    
    if (button->is_pressed) {
        bg_color = &widget->pressed_bg_color;
        border_color = &widget->active_border_color;
        text_color = &widget->xft_active_text_color;
        icon_color = &widget->xft_active_icon_color;
    } else if (button->is_active) {
        bg_color = &widget->active_bg_color;
        border_color = &widget->active_border_color;
        text_color = &widget->xft_active_text_color;
        icon_color = &widget->xft_active_icon_color;
    } else {
        bg_color = &widget->button_bg_color;
        border_color = &widget->border_color;
        text_color = &widget->xft_text_color;
        icon_color = &widget->xft_icon_color;
    }
    
    XSetForeground(widget->display, widget->gc, bg_color->pixel);
    XFillRectangle(widget->display, widget->window, widget->gc,
                   button_x, button_y, BUTTON_SIZE, BUTTON_SIZE);
    
    XSetForeground(widget->display, widget->gc, border_color->pixel);
    int border_thickness = button->is_pressed ? 3 : 2;
    
    for (int i = 0; i < border_thickness; i++) {
        XDrawRectangle(widget->display, widget->window, widget->gc,
                       button_x - i, button_y - i,
                       BUTTON_SIZE + 2 * i, BUTTON_SIZE + 2 * i);
    }
    
    int icon_area_height = BUTTON_SIZE - ICON_TEXT_SPACING - widget->text_font->height - 16;
    int icon_y = button_y + icon_area_height / 2 + widget->icon_font->ascent / 2 + 8;
    int text_y = button_y + BUTTON_SIZE - widget->text_font->descent - 8;
    
    draw_text_centered_xft(widget, button->icon, button_x, icon_y, BUTTON_SIZE, widget->icon_font, icon_color);
    draw_text_centered_xft(widget, button->text, button_x, text_y, BUTTON_SIZE, widget->text_font, text_color);
}

void draw_widget(Widget *widget) {
    if (!widget->needs_redraw) return;
    
    // Clear entire window to prevent trails
    XClearWindow(widget->display, widget->window);
    
    XSetForeground(widget->display, widget->gc, widget->bg_color.pixel);
    XFillRectangle(widget->display, widget->window, widget->gc,
                   0, 0, WIDGET_WIDTH, WIDGET_HEIGHT);
    
    for (int i = 0; i < BUTTON_COUNT; i++) {
        draw_button(widget, i);
    }
    
    XFlush(widget->display);
    widget->needs_redraw = 0;
}

int get_button_at_position(Widget *widget, int x, int y) {
    (void)widget;
    
    for (int i = 0; i < BUTTON_COUNT; i++) {
        int button_x = WIDGET_PADDING;
        int button_y = WIDGET_PADDING + i * (BUTTON_SIZE + BUTTON_MARGIN);
        
        if (x >= button_x && x <= button_x + BUTTON_SIZE &&
            y >= button_y && y <= button_y + BUTTON_SIZE) {
            return i;
        }
    }
    return -1;
}

void toggle_button(Widget *widget, int button_index) {
    if (button_index < 0 || button_index >= BUTTON_COUNT) return;
    
    Button *button = &widget->buttons[button_index];
    
    if (button->click_only) {
        // Click-only button: just execute the toggle command
        execute_command(button->toggle_command);
    } else {
        // Toggle button: change state and execute appropriate command
        button->is_active = !button->is_active;
        execute_command(button->is_active ? button->toggle_command : button->untoggle_command);
    }
    
    widget->needs_redraw = 1;
}

void cleanup_widget(Widget *widget) {
    XftColorFree(widget->display, widget->visual, widget->colormap, &widget->xft_text_color);
    XftColorFree(widget->display, widget->visual, widget->colormap, &widget->xft_active_text_color);
    XftColorFree(widget->display, widget->visual, widget->colormap, &widget->xft_icon_color);
    XftColorFree(widget->display, widget->visual, widget->colormap, &widget->xft_active_icon_color);
    XftFontClose(widget->display, widget->icon_font);
    XftFontClose(widget->display, widget->text_font);
    XftDrawDestroy(widget->xft_draw);
    
    unsigned long pixels[] = {
        widget->bg_color.pixel, widget->text_color.pixel, widget->window_border_color.pixel, widget->border_color.pixel,
        widget->button_bg_color.pixel, widget->active_bg_color.pixel, 
        widget->active_text_color.pixel, widget->active_border_color.pixel,
        widget->pressed_bg_color.pixel
    };
    XFreeColors(widget->display, widget->colormap, pixels, 9, 0);
    
    XFreeGC(widget->display, widget->gc);
    XDestroyWindow(widget->display, widget->window);
    XCloseDisplay(widget->display);
}

int check_compositor(Display *display) {
    char prop_name[32];
    snprintf(prop_name, sizeof(prop_name), "_NET_WM_CM_S%d", DefaultScreen(display));
    Atom atom = XInternAtom(display, prop_name, False);
    return XGetSelectionOwner(display, atom) != None;
}

void set_window_opacity(Widget *widget, double opacity) {
    if (!widget->has_compositor) return;
    
    Atom atom = XInternAtom(widget->display, "_NET_WM_WINDOW_OPACITY", False);
    if (atom != None) {
        unsigned long opacity_value = (unsigned long)(opacity * 0xFFFFFFFF);
        XChangeProperty(widget->display, widget->window, atom, XA_CARDINAL, 32,
                       PropModeReplace, (unsigned char*)&opacity_value, 1);
    }
}

int main() {
    Widget widget;
    XEvent event;
    
    init_widget(&widget);
    
    while (1) {
        // Process all pending X events first
        while (XPending(widget.display)) {
            XNextEvent(widget.display, &event);
            
            switch (event.type) {
                case Expose:
                    widget.needs_redraw = 1;
                    break;
                    
                case ButtonPress:
                    if (event.xbutton.button == Button1) {
                        int button_index = get_button_at_position(&widget, 
                                                                  event.xbutton.x, 
                                                                  event.xbutton.y);
                        if (button_index >= 0) {
                            widget.buttons[button_index].is_pressed = 1;
                            widget.needs_redraw = 1;
                        }
                    }
                    break;
                    
                case ButtonRelease:
                    if (event.xbutton.button == Button1) {
                        int button_index = get_button_at_position(&widget, 
                                                                  event.xbutton.x, 
                                                                  event.xbutton.y);
                        for (int i = 0; i < BUTTON_COUNT; i++) {
                            widget.buttons[i].is_pressed = 0;
                        }
                        
                        if (button_index >= 0) {
                            toggle_button(&widget, button_index);
                        }
                        widget.needs_redraw = 1;
                    }
                    break;
                    
                case ConfigureNotify:
                    if (widget.is_visible && !widget.is_closing) {
                        XMoveWindow(widget.display, widget.window, widget.current_x,
                                   (widget.screen_height - WIDGET_HEIGHT) / 2);
                    }
                    break;
            }
        }
        
        // Handle animation
        if (widget.is_animating) {
            animate_widget(&widget);
            if (widget.is_visible || widget.is_animating) {
                draw_widget(&widget);
            }
            usleep(ANIMATION_SLEEP_MS * 1000); // 16ms for ~60fps animation
            continue;
        }
        
        // Check mouse position only when not animating
        int mouse_in_zone = check_mouse_in_hover_zone(&widget);
        int mouse_over_widget = check_mouse_over_widget(&widget);
        
        if (mouse_in_zone && !widget.is_visible && !widget.is_closing) {
            start_show_animation(&widget);
        } else if (!mouse_in_zone && !mouse_over_widget && widget.is_visible && !widget.is_closing) {
            widget.mouse_in_zone = 0;
            start_close_animation(&widget);
        }
        
        // Draw if needed
        if (widget.is_visible && widget.needs_redraw) {
            draw_widget(&widget);
        }
        
        // Sleep longer when idle to achieve 0% CPU usage
        usleep(IDLE_SLEEP_MS * 1000); // 50ms when idle
    }
    
    cleanup_widget(&widget);
    return 0;
}
