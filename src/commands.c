#include "commands.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <wayland-server-core.h>

#include <wlr/types/wlr_xdg_shell.h>

#include "desktop/windows/window.h"
#include "input/seat.h"
#include "server.h"

#include "util/log.h"

#define SHELL_PATH "/bin/sh"

static void e_commands_kill_focussed_window(struct e_server* server)
{
    struct e_seat* seat = server->input_manager->seat;

    if (seat->focus_surface == NULL)
        return;

    struct e_window* window = e_window_from_surface(server, seat->focus_surface);

    if (window != NULL)
    {
        e_window_send_close(window);
        e_log_info("asked to close window");
    }
    else 
    {
        e_log_info("failed to destroy focussed window");
    }
}

static void e_commands_toggle_tiling_focussed_window(struct e_server* server)
{
    //TODO: implement
}

static void e_commands_toggle_fullscreen_focussed_window(struct e_server* server)
{
    struct e_seat* seat = server->input_manager->seat;

    if (seat->focus_surface == NULL)
        return;

    //TODO: add support for all winodw types by using the base e_window instead of wlr_xdg_toplevel
    struct wlr_xdg_toplevel* wlr_xdg_toplevel = wlr_xdg_toplevel_try_from_wlr_surface(seat->focus_surface);

    if (wlr_xdg_toplevel != NULL)
    {
        wlr_xdg_toplevel_set_fullscreen(wlr_xdg_toplevel, !wlr_xdg_toplevel->current.fullscreen);
        e_log_info("requested to toggle fullscreen wlr_xdg_toplevel, title: %s", wlr_xdg_toplevel->title);
    }
    else 
    {
        e_log_info("failed to toggle fullscreen of focussed window");
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

void e_commands_parse(struct e_server* server, const char* command)
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
        {
            e_log_error("Command is too short, not enough arguments given");
        }
        else 
        {
            e_commands_exec_as_new_process((command + commandTypeLength + 1));
        }
    }
    //type is exit
    else if (strcmp(argument, "exit") == 0)
    {
        //will quit EstrogenWL
        wl_display_terminate(server->display);
    }
    //type is kill
    else if (strcmp(argument, "kill") == 0)
    {
        e_commands_kill_focussed_window(server);
    }
    //TODO: toggle_fullscreen & toggle_tiling are currently placeholders
    else if (strcmp(argument, "toggle_fullscreen") == 0)
    {
        e_commands_toggle_fullscreen_focussed_window(server);
    }
    else if (strcmp(argument, "toggle_tiling") == 0)
    {
        e_commands_toggle_tiling_focussed_window(server);
    }
    else 
    {
        e_log_error("Invalid command type!");
    }
}