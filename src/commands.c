#include "commands.h"

#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <sys/wait.h>

#include <wayland-server-core.h>

#include "desktop/desktop.h"
#include "desktop/output.h"

#include "desktop/tree/container.h"
#include "desktop/tree/workspace.h"

#include "desktop/views/view.h"

#include "input/cursor.h"
#include "input/seat.h"

#include "util/list.h"
#include "util/log.h"

#include "server.h"

//TODO: this really needs to be updated and be rewritten in the same style as the rest of the code, because WOW this is garbage

#define SHELL_PATH "/bin/sh"

static void e_commands_kill_focused_view(struct e_server* server)
{
    /*
    struct e_view* view = e_desktop_focused_view(server);

    if (view != NULL)
    {
        e_view_send_close(view);

        e_log_info("asked to close view, title: %s", view->title == NULL ? "no name" : view->title);
    }
    else 
    {
        e_log_info("failed to close focused view");
    }
        */
}

static void e_commands_toggle_tiling_focused_view(struct e_server* server)
{
    /*
    struct e_view* view = e_desktop_focused_view(server);

    if (view != NULL)
    {
        e_view_set_tiled(view, !view->tiled);

        e_log_info("setted tiling of view, title: %s", view->title == NULL ? "no name" : view->title);
    }
    else 
    {
        e_log_info("failed to set tiling of focused view");
    }
        */
}

static void e_commands_switch_tiling_mode(struct e_server* server)
{
    assert(server);

    //struct e_view* view = e_desktop_focused_view(server);
    /*

    if (view == NULL || view->container.parent == NULL)
        return;

    struct e_tree_container* parent_container = view->container.parent;

    if (parent_container->tiling_mode == E_TILING_MODE_HORIZONTAL)
        parent_container->tiling_mode = E_TILING_MODE_VERTICAL;
    else
        parent_container->tiling_mode = E_TILING_MODE_HORIZONTAL;

    e_tree_container_arrange(parent_container);
    */
}

static void e_commands_toggle_fullscreen(struct e_server* server)
{
    struct e_view_container* view_container = e_desktop_focused_view_container(server);

    if (view_container == NULL)
        return;

    struct e_container* container = &view_container->base;

    if (container->workspace != NULL)
    {
        if (container->workspace->fullscreen_container != container)
            e_workspace_change_fullscreen_container(container->workspace, container);
        else
            e_workspace_change_fullscreen_container(container->workspace, NULL);

        e_workspace_rearrange(container->workspace);
        
        e_log_info("toggle fullscreen mode of container, fullscreen: %i", container->fullscreen);
    }
    else 
    {
        e_log_error("e_commands_toggle_fullscreen: failed to toggle fullscreen mode of container!");
    }
}

static void e_commands_exec_as_new_process(const char* command)
{
    e_log_info("RUNNING COMMAND AS NEW SHELL PROCESS: %s", command);

    //clone process, process id is given to current process and 0 is given to new process
    int pid = fork();

    if (pid < 0)
    {
        e_log_error("e_commands_exec_as_new_process: failed to fork");
        return;
    }

    if (pid == 0)
    {
        //new process starts here

        //according to the wikipedia page on forking _exit must be used instead of exit

        //avoid: https://en.wikipedia.org/wiki/Zombie_process
        //thanks wikipedia and labwc and sway

        setsid(); //orphan new process to avoid zombie processes
        execl(SHELL_PATH, SHELL_PATH, "-c", command, NULL);
        _exit(1); //exit process if execl failed
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
            e_log_error("Command is too short, not enough arguments given");
        else
            e_commands_exec_as_new_process((command + commandTypeLength + 1));
    }
    //type is exit
    else if (strcmp(argument, "exit") == 0)
    {
        //will quit EstrogenWL
        e_server_terminate(server);
    }
    //type is kill
    else if (strcmp(argument, "kill") == 0)
    {
        e_commands_kill_focused_view(server);
    }
    //TODO: toggle_fullscreen & toggle_tiling are currently placeholders
    else if (strcmp(argument, "toggle_fullscreen") == 0)
    {
        e_commands_toggle_fullscreen(server);
    }
    else if (strcmp(argument, "toggle_tiling") == 0)
    {
        e_commands_toggle_tiling_focused_view(server);
    }
    //TODO: switch_tiling_mode is a placeholder name
    else if (strcmp(argument, "switch_tiling_mode") == 0)
    {
        e_commands_switch_tiling_mode(server);
    }
    else if (strcmp(argument, "maximize") == 0)
    {
        e_log_info("maximize");
        //TODO: maximize
    }
    //TODO: next_workspace is for testing only, remove
    else if (strcmp(argument, "next_workspace") == 0)
    {
        struct e_output* output = e_desktop_hovered_output(server);

        struct e_workspace* workspace = output->active_workspace;

        if (workspace == NULL)
        {
            e_log_error("no workspace");
            return;
        }

        int i = e_list_find_index(&output->workspace_group.workspaces, workspace);

        e_output_display_workspace(output, e_list_at(&output->workspace_group.workspaces, (i + 1) % output->workspace_group.workspaces.count));
        e_cursor_set_focus_hover(server->seat->cursor);
        e_log_info("output workspace index: %i", (i + 1) % output->workspace_group.workspaces.count);
    }
    //TODO: testing only, remove
    else if (strcmp(argument, "move_to_next_workspace") == 0)
    {
        /*
        struct e_view* focused_view = e_desktop_focused_view(server);

        if (focused_view == NULL)
            return;

        struct e_output* output = e_desktop_hovered_output(server);

        struct e_workspace* workspace = output->active_workspace;

        if (workspace == NULL)
        {
            e_log_error("no workspace");
            return;
        }

        int i = e_list_find_index(&output->workspace_group.workspaces, workspace);

        //e_view_move_to_workspace(focused_view, e_list_at(&output->workspace_group.workspaces, (i + 1) % output->workspace_group.workspaces.count));
        e_cursor_set_focus_hover(server->seat->cursor);
        e_log_info("view workspace index: %i", (i + 1) % output->workspace_group.workspaces.count);
        */
    }
    else 
    {
        e_log_error("Invalid command type!");
    }
}
