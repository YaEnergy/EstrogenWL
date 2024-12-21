#include "commands.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-server-core.h>

#include "server.h"

#include "util/log.h"

#define SHELL_PATH "/bin/sh"

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
            e_commands_run_command_as_new_process((command + commandTypeLength + 1));
        }
    }
    //type is exit
    else if (strcmp(argument, "exit") == 0)
    {
        //will quit EstrogenCompositor
        wl_display_terminate(server->display);
    }
    //type is kill
    else if (strcmp(argument, "kill") == 0)
    {
        e_commands_kill_focussed_window(server);
    }
    else 
    {
        e_log_error("Invalid command type!");
    }
}

void e_commands_run_command_as_new_process(const char* command)
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

void e_commands_kill_focussed_window(struct e_server* server)
{
    //TODO: implement
}

void e_commands_switch_focussed_window_type(struct e_server* server)
{
    //TODO: implement
}