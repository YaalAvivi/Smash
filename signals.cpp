#include <iostream>
#include <unistd.h>
#include <signal.h>
#include "signals.h"
#include "Commands.h"
#include "math.h"
#include <time.h>

using namespace std;

void ctrlZHandler(int sig_num) {
  SmallShell& smash = SmallShell::getInstance();
  cout << "smash: got ctrl-Z"<<endl;
  if(smash.running_pid !=0)
  {
    kill (smash.running_pid,SIGSTOP);
    cout << "smash: process "<<smash.running_pid<<" was stopped"<<endl;
    smash.job_list.addJob(smash.running_cmd,smash.running_pid,true,true);
  }
}

void ctrlCHandler(int sig_num) {
  cout << "smash: got ctrl-C"<<endl;
  SmallShell& smash = SmallShell::getInstance();
  if(smash.running_pid !=0)
  {
    kill (smash.running_pid, SIGKILL);
    cout << "smash: process "<<smash.running_pid<<" was killed"<<endl;
  }
}

void alarmHandler(int sig_num) {
  cout << "smash: got an alarm"<<endl;
  SmallShell& smash = SmallShell::getInstance();
  multiset<TimedoutObj>::const_iterator it=smash.timed_out_set.begin();
  string cmd = it->command_line;
  pid_t pid = it->pid;
  smash.timed_out_set.erase(it);
  if(pid!=smash.getShellPid())
  {
    //START CHANGING
    smash.job_list.removeFinishedJobs();
    //END OF CHANGING
    if(smash.job_list.JobExistByPid(pid) || smash.running_pid==pid){
      cout << "smash: "<<cmd<<" timed out!"<<endl;
      kill(pid,SIGKILL);
    }
  }
  if (!smash.timed_out_set.empty())
    alarm(abs(difftime(time(0),smash.timed_out_set.begin()->time_to_execute)));
}

