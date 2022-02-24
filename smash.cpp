#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

int main(int argc, char* argv[]) {
    if(signal(SIGTSTP , ctrlZHandler)==SIG_ERR) {
        perror("smash error: failed to set ctrl-Z handler");
    }
    if(signal(SIGINT , ctrlCHandler)==SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }
    struct sigaction new_action;

    /* Set up the structure to specify the new action. */
    new_action.sa_handler = alarmHandler;
    new_action.sa_flags =SA_RESTART; /*termination_handler*/
    if(sigaction(SIGALRM,&new_action,NULL)==-1)
    {
        perror("smash error: failed to set alarm handler");
    }
    
    //TODO: setup sig alarm handler

    SmallShell& smash = SmallShell::getInstance();
    while(true) {
        smash.printPromptName();
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        smash.executeCommand(cmd_line.c_str());
    }
    return 0;
}