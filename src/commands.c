#include "commands.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <wayland-server-core.h>

#include <wlr/types/wlr_xdg_shell.h>

#include "desktop/desktop.h"
#include "desktop/tree/container.h"
#include "desktop/views/view.h"
#include "desktop/views/window.h"
#include "input/seat.h"

#include "util/log.h"

#define SHELL_PATH "/bin/sh"

static void e_commands_kill_focused_view(struct e_desktop* desktop)
{
    struct e_seat* seat = desktop->seat;

    if (seat->focus_surface == NULL)
        return;

    struct e_view* view = e_seat_focused_view(desktop->seat);

    if (view != NULL)
    {
        e_view_send_close(view);

        e_log_info("asked to close view, title: %s", view->title == NULL ? "no name" : view->title);
    }
    else 
    {
        e_log_info("failed to close focused view");
    }
}

static void e_commands_toggle_tiling_focused_window(struct e_desktop* desktop)
{
    struct e_seat* seat = desktop->seat;

    if (seat->focus_surface == NULL)
        return;

    struct e_window* window = e_seat_focused_window(desktop->seat);

    if (window != NULL)
    {
        e_window_set_tiled(window, !window->tiled);

        e_log_info("setted tiling of window, title: %s", window->title == NULL ? "no name" : window->title);
    }
    else 
    {
        e_log_info("failed to set tiling of focused window");
    }
}

static void e_commands_switch_tiling_mode(struct e_desktop* desktop)
{
    assert(desktop);

    struct e_seat* seat = desktop->seat;

    if (seat->focus_surface == NULL)
        return;

    struct e_window* window = e_seat_focused_window(desktop->seat);

    if (window == NULL || window->base.parent == NULL)
        return;

    struct e_tree_container* parent_container = window->base.parent;

    if (parent_container->tiling_mode == E_TILING_MODE_HORIZONTAL)
        parent_container->tiling_mode = E_TILING_MODE_VERTICAL;
    else
        parent_container->tiling_mode = E_TILING_MODE_HORIZONTAL;

    e_tree_container_arrange(parent_container);
}

static void e_commands_toggle_fullscreen_focused_window(struct e_desktop* desktop)
{
    struct e_seat* seat = desktop->seat;

    if (seat->focus_surface == NULL)
        return;

    //TODO: add support for all winodw types by using the base e_view instead of wlr_xdg_toplevel
    struct wlr_xdg_toplevel* wlr_xdg_toplevel = wlr_xdg_toplevel_try_from_wlr_surface(seat->focus_surface);

    if (wlr_xdg_toplevel != NULL)
    {
        wlr_xdg_toplevel_set_fullscreen(wlr_xdg_toplevel, !wlr_xdg_toplevel->current.fullscreen);
        e_log_info("requested to toggle fullscreen wlr_xdg_toplevel, title: %s", wlr_xdg_toplevel->title);
    }
    else 
    {
        e_log_info("failed to toggle fullscreen of focussed view");
    }   
}

static void e_commands_exec_as_new_process(const char* command)
{
    e_log_info("RUNNING COMMAND AS NEW SHELL PROCESS: %s", command);

    //duplicate this process, and check if successful and that this is the new process  
    if (fork() == 0)
    {
        // new process starts here

        //unsure why I need to execute shell to run shell again, but otherwise program acts strange
        execl(SHELL_PATH, SHELL_PATH, "-c", command, NULL);

        //should never be reached
        return;
    }
}

void e_commands_parse(struct e_desktop* desktop, const char* command)
{
    int commandLength = strlen(command);
    char arguments[commandLength + 1];
    strncpy(arguments, command, commandLength + 1);

    //get first argument: commandType
    char* argument = strtok(arguments, " ");
    int commandTypeLength = strlen(argument);

    e_log_info("Command type: %s;", argument);

    //type is exec
    if (strcmp(argument, "exec") == 0)
    {
        // use remaining arguments, skipping command type arg & space

        //don't go past the size of the commmand
        if (commandLength - (commandTypeLength + 1) <= 0)
            e_log_error("Command is too short, not enough arguments given");
        else
            e_commands_exec_as_new_process((command + commandTypeLength + 1));
    }
    //type is exit
    else if (strcmp(argument, "exit") == 0)
    {
        //will quit EstrogenWL
        wl_display_terminate(desktop->display);
    }
    //type is kill
    else if (strcmp(argument, "kill") == 0)
    {
        e_commands_kill_focused_view(desktop);
    }
    //TODO: toggle_fullscreen & toggle_tiling are currently placeholders
    else if (strcmp(argument, "toggle_fullscreen") == 0)
    {
        e_commands_toggle_fullscreen_focused_window(desktop);
    }
    else if (strcmp(argument, "toggle_tiling") == 0)
    {
        e_commands_toggle_tiling_focused_window(desktop);
    }
    //TODO: switch_tiling_mode is a placeholder name
    else if (strcmp(argument, "switch_tiling_mode") == 0)
    {
        e_commands_switch_tiling_mode(desktop);
    }
    else 
    {
        e_log_error("Invalid command type!");
    }
}