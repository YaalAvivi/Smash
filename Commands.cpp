#include <unistd.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


const std::string WHITESPACE= "\n\r\t\f\v ";
using namespace std;

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif



string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for(std::string s; iss >> s; ) {
    args[i] = (char*)malloc(s.length()+1);
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

ExternalCommand::ExternalCommand(const char* cmd_line) : Command(cmd_line),background_command(false),
                                                        cmd_line(cmd_line), time_out_cmd(false) , time_of_timeout(-1){
  background_command=_isBackgroundComamnd(cmd_line);
  time_out_cmd= _isTimeoutCommand(this->cmd_line);
  if(time_out_cmd)
  {
    removeTimeout(this->cmd_line);
  }
}

void ExternalCommand::execute()
{
  SmallShell& smash = SmallShell::getInstance();
  pid_t cur_pid = fork();
  if (cur_pid==0) //son
  {
    setpgrp();
    char sent_cmd_line[cmd_line.size()+1];
    strcpy(sent_cmd_line,cmd_line.c_str());
    _removeBackgroundSign(sent_cmd_line);
    char arg1[10];
    arg1[9] = '\0';
    std::string arg1_str("/bin/bash");
    for (unsigned int i=0; i<arg1_str.size(); i++)
    {
      arg1[i]=arg1_str[i];
    }
    char flag[3];
    flag[2] = '\0';
    flag[0]='-';
    flag[1]='c';
    char* args[4]={arg1,flag,sent_cmd_line,NULL};
    if(execvp(args[0],args) == -1)
    {
      perror("smash error: execvep failed");
      exit(1);
    }
  }
  else if(cur_pid>0) //father
  {
    if(time_out_cmd)
    {
      TimedoutObj new_timed_obj(original_cmd,cur_pid,time_of_timeout);
      smash.timed_out_set.insert(new_timed_obj);
      alarm(abs(difftime(time(0),smash.timed_out_set.begin()->time_to_execute)));
    }
    if(background_command==false)
      waitForForegroundProccess(cur_pid,this);
    else
      smash.job_list.addJob(this,cur_pid);
  }
  else //fork failed
    perror("smash error: fork failed");
}

void SmallShell::printPromptName() const
{
  std::cout<<prompt_name<<"> ";
}

void SmallShell::changePromptName(const std::string& new_name)
{
  if (new_name == "")
  {
    prompt_name = "smash";
  }
  else{
    prompt_name = new_name;
  }
}

pid_t SmallShell::getShellPid() const
{
  return shell_pid;
}

std::string SmallShell::getFormerPath() const
{
  return former_path;
}

void SmallShell::setFormerPath(const std::string& path)
{
  former_path= path;
}

SmallShell::SmallShell():  shell_pid(getpid()) , prompt_name("smash")
                          ,former_path("")  ,running_pid(0),running_cmd(NULL), job_list(),timed_out_set() ,commands_to_delete(){}

SmallShell::~SmallShell()
{
  for (vector<JobsList::JobEntry>::iterator it =job_list.smash_jobs.begin(); it!=job_list.smash_jobs.end(); it++)
  {
    delete it->cmd;
  }
  DeleteCommands();
}

bool RedirectionCmdLine(const char* cmd_line)
{
  return ((string(cmd_line).find_first_of(">"))!=string::npos);
}

bool PipeCmdLine(const char* cmd_line)
{
  return ((string(cmd_line).find_first_of("|"))!=string::npos);
}


bool _isTimeoutCommand(string& cmd_line)
{
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
  return (firstWord.compare("timeout") == 0);
}

void ExternalCommand::removeTimeout(string& cmd_line)
{
  char* arg[COMMAND_MAX_ARGS];
  int args_num=_parseCommandLine(cmd_line.c_str(),arg);
  int timeout=std::stoi(arg[1]);
  string new_cmd="";
  for(int i=2;i<args_num;i++)
  {
    new_cmd+=arg[i];
    new_cmd+=" ";
  }
  cmd_line=new_cmd;
  time_of_timeout=timeout;
  for(int i=0;i<args_num;i++)
    free(arg[i]);
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
  if(RedirectionCmdLine(cmd_line)){
    return new RedirectionCommand(cmd_line);
  }
  if(PipeCmdLine(cmd_line)){
    return new PipeCommand(cmd_line);
  }
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
  if(*(firstWord.rbegin())=='&')
    firstWord=firstWord.substr(0,firstWord.size()-1);

  if (firstWord.compare("chprompt") == 0) {
    return new changePromptCommand(cmd_line);
  }
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  else if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("cd")==0){
    return new ChangeDirCommand(cmd_line);
  }
  else if (firstWord.compare("jobs")==0){
    return new JobsCommand(cmd_line, &job_list);
  }
  else if (firstWord.compare("kill")==0){
    return new KillCommand(cmd_line,&job_list);
  }
  else if(firstWord.compare("fg")==0){
    return new ForegroundCommand(cmd_line, &job_list);
  }
  else if(firstWord.compare("bg")==0){
    return new BackgroundCommand(cmd_line, &job_list);
  }
  else if(firstWord.compare("quit")==0){
    return new QuitCommand(cmd_line, &job_list);
  }
  else if(firstWord.compare("cat")==0){
    return new CatCommand(cmd_line);
  }
  else {
    return new ExternalCommand(cmd_line);
  }
  return nullptr;
}

Command::Command(const char* cmd_line,bool isBuiltIn) : original_cmd(cmd_line),jobID()
{
  SmallShell& smash = SmallShell::getInstance();
  jobID=smash.job_list.JobListGetMaxID()+1;
  if(isBuiltIn)
    smash.commands_to_delete.push_back(this);
}

BuiltInCommand::BuiltInCommand(const char* cmd_line) : Command(cmd_line,true){}

void SmallShell::executeCommand(const char *cmd_line) 
{
  job_list.removeFinishedJobs();
  Command* cmd = CreateCommand(cmd_line);
  cmd->execute();
  DeleteCommands();
}

changePromptCommand::changePromptCommand(const char* cmd_line) : BuiltInCommand(cmd_line)
{
  char* arg[COMMAND_MAX_ARGS];
  char sent_cmd_line[string(cmd_line).size()+1];
  strcpy(sent_cmd_line,cmd_line);
  _removeBackgroundSign(sent_cmd_line);
  int args_num = _parseCommandLine(sent_cmd_line , arg);
  if (args_num<2)
    prompt="";
  else
    prompt=arg[1];
  for(int i=0;i<args_num;i++)
    free(arg[i]);
}

void changePromptCommand::execute()
{
  SmallShell& smash = SmallShell::getInstance();
  smash.changePromptName(prompt);
}

ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line){}

void ShowPidCommand::execute()
{
  SmallShell& smash = SmallShell::getInstance();
  cout<< "smash pid is "<< smash.getShellPid() << endl;
}

GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line){}

void GetCurrDirCommand::execute()
{
  char* temp(get_current_dir_name());
  if(temp==NULL)
  {
    perror("smash error: get_current_dir_name failed");
    return;
  }
  std::string path(temp);
  cout<< path<<endl;
}

CatCommand::CatCommand(const char* cmd_line) : BuiltInCommand(cmd_line)
{
  char sent_cmd_line[string(cmd_line).size()+1];
  strcpy(sent_cmd_line,cmd_line);
  _removeBackgroundSign(sent_cmd_line);
  args_num = _parseCommandLine(sent_cmd_line , args);
}

void CatCommand::execute()
{
  if(args_num<2)
  {
    cerr << "smash error: cat: not enough arguments" <<endl;
    goto end;
  }
  for(int i=1;i<args_num;i++)
  {
    int fd;
    if ((fd=open(args[i],O_RDONLY))==-1)
    {
      perror("smash error: open failed");
      goto end;
    }
    else
    {
      char buffer[100];
      ssize_t ret;
      while ((ret = read(fd, buffer, 100) )>0)
      {
        if((write(1,buffer,ret)==-1))
        {
          perror("smash error: write failed");
          goto cnt;
        }
      }
      if (ret==-1)
      {
        perror("smash error: read failed");
      }
    }
    cnt: ;
    if (close(fd) ==-1)
    {
      perror("smash error: close failed");
    }
  }
  end: ;
  for(int i=0;i<args_num;i++)
      free(args[i]);
}

ChangeDirCommand::ChangeDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line)
{
  char* arg[COMMAND_MAX_ARGS];
  char sent_cmd_line[string(cmd_line).size()+1];
  strcpy(sent_cmd_line,cmd_line);
  _removeBackgroundSign(sent_cmd_line);
  args_num = _parseCommandLine(sent_cmd_line , arg);
  if (args_num>=2)
  {
    path=arg[1];
  }
  else
  {
    path = "";
  }
  for(int i=0;i<args_num;i++)
    free(arg[i]);
}

void ChangeDirCommand::execute()
{
  if(args_num==1)
  {
    return;
  }
  else if(args_num>2)
  {
    cerr<<"smash error: cd: too many arguments" <<endl;
    return;
  }
  SmallShell& smash = SmallShell::getInstance();
  char* temp(get_current_dir_name());
  if(temp==NULL)
  {
    perror("smash error: get_current_dir_name failed");
    return;
  }
  std::string cur_path(temp);
  if(path.compare("-")==0)
  {
    path = smash.getFormerPath();
    if(path.compare("")==0)
    {
      cerr<<"smash error: cd: OLDPWD not set" <<endl;
      return;
    }
  }
  const char* char_path=path.c_str();
  if(chdir(char_path)==-1)
  {
    perror("smash error: chdir failed");
    return;
  }
  smash.setFormerPath(cur_path);
}

JobsCommand::JobsCommand(const char* cmd_line, JobsList* jobs): BuiltInCommand(cmd_line), jobs(jobs){}

void JobsCommand::execute()
{
  jobs->printJobsList();
}

bool isValidKillInput(char** arg)
{
  //CHECK IF SIGNAL IS VALID
  std::string signal_str(arg[1]);
  if(signal_str.size()<=1 || signal_str.at(0)!='-')
  {
    return false;
  }
  if(!isStringNumber(signal_str.substr(1)))
  {
    return false;
  }
  //CHECK IF JOB IS VALID
  std::string job_str(arg[2]);
  if(job_str.size()<=0)
  {
    return false;
  }
  if(!isStringNumber(job_str))
  {
    return false;
  }
  try 
  {
    std::stoi(job_str);
  }
  catch(...)
  {
    return false;
  }
  return true;
}

KillCommand::KillCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line),
                                                                 jobs(jobs), valid_input(true){
  char* arg[COMMAND_MAX_ARGS];
  char sent_cmd_line[string(cmd_line).size()+1];
  strcpy(sent_cmd_line,cmd_line);
  _removeBackgroundSign(sent_cmd_line);
  int args_num = _parseCommandLine(sent_cmd_line , arg);
  if(args_num!=3)
  {
    valid_input=false;
  }
  else 
  {
    valid_input=isValidKillInput(arg);
    if(valid_input)
    {
      signal_num = std::stoi(string(arg[1]).substr(1));
      job_id = std::stoi(arg[2]);
    }
  }
  for(int i=0;i<args_num;i++)
    free(arg[i]);
}

void KillCommand::execute()
{
  if(valid_input==false)
  {
    cerr<< "smash error: kill: invalid arguments" <<endl;
    return;
  }
  const JobsList::JobEntry* ptr = jobs->getJobById(job_id);
  if(!ptr)
  {
    cerr << "smash error: kill: job-id " << job_id << " does not exist" <<endl;
    return;
  }
  if (kill (ptr->job_pid,signal_num)== -1)
  {
    perror("smash error: kill failed");
    return;
  }
  cout<< "signal number " << signal_num << " was sent to pid "<<ptr->job_pid<< endl;
  if(signal_num==SIGSTOP)
    jobs->ChangeStatusProcess(job_id,stopped);
}

void JobsList::ChangeStatusProcess(int job_id,status new_status)
{
  for (vector<JobsList::JobEntry>::iterator it =smash_jobs.begin(); it!=smash_jobs.end(); it++)
  {
    if (it->job_id == job_id)
    {
      it->job_status=new_status;
      return;
    }
  }
}

void JobsList::addJob(Command* cmd, pid_t job_pid, bool isStopped,bool isForground)
{
  removeFinishedJobs();
  status cmd_status=isStopped? stopped:running_back;
  JobsList::JobEntry new_job(cmd->jobID, job_pid,time(NULL),cmd_status,cmd->original_cmd, cmd);
  smash_jobs.push_back(new_job);
}

int JobsList::JobListGetMaxID(bool stopped) const
{
  int max=0;
  for (vector<JobEntry>::const_iterator it =smash_jobs.begin(); it!=smash_jobs.end(); it++)
  {
    if (it->job_id>max)
      if(!stopped || (stopped && it->job_status==stopped))
        max=it->job_id;
  }
  return max;
}

void JobsList::printJobsList()
{
  removeFinishedJobs();
  std::sort (smash_jobs.begin(), smash_jobs.end());
  for (vector<JobEntry>::iterator it =  this->smash_jobs.begin(); it !=  this->smash_jobs.end(); it++)
  {
    if (it->job_status == stopped)
    {
    std::cout << "[" <<it->job_id<<"] "<< it->cmd_call << " : " << it->job_pid << " " << difftime(time(NULL),it->inserted_time) <<" secs"<< " (stopped)" <<std::endl;
    }
    else
    {
      std::cout << "[" <<it->job_id<<"] "<< it->cmd_call << " : " << it->job_pid << " " << difftime(time(NULL),it->inserted_time) <<" secs"<< std::endl;
    }
  }
}

void JobsList::removeFinishedJobs()
{
  vector<JobEntry>::iterator it = smash_jobs.begin();
  while (it != smash_jobs.end())
  {
    if(waitpid(it->job_pid,NULL,WNOHANG)==it->job_pid)
    {
      delete it->cmd;
      it=smash_jobs.erase(it);
    }
    else
      it++;
  }
}

JobsList::JobEntry* JobsList::getJobById(int jobId)
{
  for (vector<JobsList::JobEntry>::iterator it = smash_jobs.begin(); it != smash_jobs.end(); it++)
  {
    if (it->job_id == jobId)
    {
      return &(*it);
    }
  }
  return NULL;
}

bool JobsList::JobExistByPid(pid_t jobPid)
{
  for (vector<JobsList::JobEntry>::iterator it = smash_jobs.begin(); it != smash_jobs.end(); it++)
  {
    if (it->job_pid == jobPid)
    {
      return true;
    }
  }
  return false;
}

void JobsList::removeJobById(int jobId)
{
  for (vector<JobsList::JobEntry>::iterator it = smash_jobs.begin(); it != smash_jobs.end(); it++)
  {
    if (it->job_id == jobId)
    {
      smash_jobs.erase(it);
      return;
    }
  }
}

void JobsList::removeJobByPid(pid_t pid)
{
  for (vector<JobsList::JobEntry>::iterator it = smash_jobs.begin(); it != smash_jobs.end(); it++)
  {
    if (it->job_pid == pid)
    {
      smash_jobs.erase(it);
      return;
    }
  }
}

JobsList::JobEntry::JobEntry(int job_id, pid_t job_pid, time_t inserted_time,
                            status job_status, std::string& cmd_call,Command* cmd)
                            : job_id(job_id), job_pid(job_pid), inserted_time(inserted_time),
                             job_status(job_status), cmd_call(cmd_call),cmd(cmd){}

JobsList::JobEntry* JobsList::getLastStoppedJob(int *jobId)
{
  int max_id= JobListGetMaxID(true);
  for (vector<JobsList::JobEntry>::iterator it = smash_jobs.begin(); it != smash_jobs.end(); it++)
  {
    if (it->job_id == max_id)
    {
      return &(*it);
    }
  }
  return NULL;
}

bool JobsList::JobEntry::operator<(const JobsList::JobEntry other) const
{
  return job_id<other.job_id;
}

bool isStringNumber(std::string str)
{
  if(str.size()==0)
  {
    return false;
  }
  else if(str[0]=='-')
  {
    return str.substr(1).find_first_not_of("0123456789")==std::string::npos;
  }
  else
  {
    return str.find_first_not_of("0123456789")==std::string::npos;
  }
}


ForegroundCommand::ForegroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line),err(no_err), jobs(jobs)
{
  char* arg[COMMAND_MAX_ARGS];
  char sent_cmd_line[string(cmd_line).size()+1];
  strcpy(sent_cmd_line,cmd_line);
  _removeBackgroundSign(sent_cmd_line);
  int args_num = _parseCommandLine(sent_cmd_line , arg);
  if (args_num>2)
  {
    err=invalid_arg;
  }
  else if(args_num==1 && jobs->smash_jobs.empty())
  {
    err=list_empty;
  }
  else
    {
      if(args_num==1)
        job_id = jobs->JobListGetMaxID();
      else
      {
        std::string job_str(arg[1]);
        if(isStringNumber(job_str))
        {
          job_id = std::stoi(job_str);
        }
        else
        {
          err=invalid_arg;
          return ;
        }
        if (jobs->getJobById(job_id)==NULL)
          err = not_exist;
      }
  }
  for(int i=0;i<args_num;i++)
    free(arg[i]);
}

void ForegroundCommand::execute()
{
  if (err==invalid_arg)
  {
    cerr << "smash error: fg: invalid arguments" << endl;
    return;
  }
  else if( err == list_empty)
  {
    cerr << "smash error: fg: jobs list is empty" << endl;
    return;
  }
  else if(err == not_exist)
  {
    cerr << "smash error: fg: job-id "<< job_id << " does not exist" << endl;
    return;
  }
  else
  {
    const JobsList::JobEntry* ptr_job = jobs->getJobById(job_id);
    std::cout << ptr_job->cmd_call << " : " << ptr_job->job_pid << std::endl;
    kill (ptr_job->job_pid,SIGCONT);
    pid_t job_pid = ptr_job->job_pid;
    waitForForegroundProccess(job_pid,ptr_job->cmd);
  }
}

void waitForForegroundProccess(pid_t pid, Command* cmd)
{
  SmallShell& smash = SmallShell::getInstance();
  smash.running_pid=pid;
  smash.running_cmd=cmd;
  smash.job_list.removeJobByPid(pid);
  waitpid(pid, NULL, WUNTRACED);
  smash.running_pid=0;
  smash.running_cmd=NULL;
}

BackgroundCommand::BackgroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line),err(no_err), jobs(jobs)
{
  char* arg[COMMAND_MAX_ARGS];
  char sent_cmd_line[string(cmd_line).size()+1];
  strcpy(sent_cmd_line,cmd_line);
  _removeBackgroundSign(sent_cmd_line);
  int args_num = _parseCommandLine(sent_cmd_line , arg);
  if (args_num>2)
  {
    err=invalid_arg;
  }
  else 
    {
      if(args_num==1)
      { 
        if(jobs->getLastStoppedJob(NULL)==NULL)
        {
          err=stopped_list_empty;
          return;
        }
        job_id = jobs->JobListGetMaxID();
      }
      else
      {
        std::string job_str(arg[1]);
        if(isStringNumber(job_str))
        {
          job_id = std::stoi(job_str);
        }
        else
        {
          err=invalid_arg;
          return ;
        }
      }
    if (jobs->getJobById(job_id)==NULL)
      err = not_exist;
    else if (jobs->getJobById(job_id)->job_status==running_back)
    {
      err=already_bg;
    }
  }
  for(int i=0;i<args_num;i++)
    free(arg[i]);
}

void BackgroundCommand::execute()
{
  if (err==invalid_arg)
  {
    cerr << "smash error: bg: invalid arguments" << endl;
    return;
  }
  else if( err == stopped_list_empty)
  {
    cerr << "smash error: bg: there is no stopped jobs to resume" << endl;
    return;
  }
  else if(err == not_exist)
  {
    cerr << "smash error: bg: job-id "<< job_id << " does not exist" << endl;
    return;
  }
  else if(err == already_bg)
  {
    cerr << "smash error: bg: job-id "<< job_id << " is already running in the background" << endl;
    return;
  }
  else
  {
    JobsList::JobEntry* ptr_job = jobs->getJobById(job_id);
    std::cout << ptr_job->cmd_call << " : " << ptr_job->job_pid << std::endl;
    kill (ptr_job->job_pid,SIGCONT);
    ptr_job->job_status=running_back;
  }
}

QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs), kill_flag(false)
{
  char* arg[COMMAND_MAX_ARGS];
  char sent_cmd_line[string(cmd_line).size()+1];
  strcpy(sent_cmd_line,cmd_line);
  _removeBackgroundSign(sent_cmd_line);
  int args_num = _parseCommandLine(sent_cmd_line , arg);
  if (args_num>1 && std::string(arg[1]).compare("kill")==0)
  {
    kill_flag=true;
  }
  for(int i=0;i<args_num;i++)
    free(arg[i]);
}

void QuitCommand::execute()
{
  if(kill_flag)
  {
    cout<<"smash: sending SIGKILL signal to " <<jobs->smash_jobs.size()<< " jobs:" << endl;
    for (vector<JobsList::JobEntry>::iterator it =  jobs->smash_jobs.begin(); it !=  jobs->smash_jobs.end(); it++)
      {
      std::cout <<it->job_pid<<": "<< it->cmd_call << std::endl;
      kill(it->job_pid,SIGKILL);
      }
  }
  exit(0);
}

string FixCmdRedirection(const char* cmd)
{
  string old_cmd=cmd;
  size_t pos=old_cmd.find_first_of(">");
  if(pos==std::string::npos)
  {
    return cmd;
  }
  if(old_cmd.at(pos+1)=='>')
    old_cmd.insert(pos+2," ");
  else
    old_cmd.insert(pos+1," ");
  old_cmd.insert(pos," ");
  return old_cmd;
}

RedirectionCommand::RedirectionCommand(const char* cmd_line) : Command(cmd_line), updated_cmd_line()
{
  string curr_arg;
  string fixed_cmd_str=FixCmdRedirection(cmd_line);
  const char* fixed_cmd=fixed_cmd_str.c_str();
  char* arg[COMMAND_MAX_ARGS];
  int args_num = _parseCommandLine(fixed_cmd , arg);
  for(int i=0;i<args_num;i++)
  {
    curr_arg = arg[i];
    if(curr_arg.compare(">")==0)
    {
      overrideOutput=true;
      i++;
      curr_arg = arg[i];
      output_file=curr_arg;
    }
    else if(curr_arg.compare(">>")==0)
    {
      overrideOutput=false;
      i++;
      curr_arg = arg[i];
      output_file=curr_arg;
    }
    else{
      updated_cmd_line+=curr_arg;
      updated_cmd_line+=" ";
    }
  }
  for(int i=0;i<args_num;i++)
    free(arg[i]);
}

 void RedirectionCommand::execute()
 {
   int fd,old_fd;
   if(overrideOutput) // >
    {
      if ((fd=open(output_file.c_str(),O_CREAT |O_RDWR |O_TRUNC,S_IRWXO | S_IRWXU | S_IRWXG))==-1)
      {
        perror("smash error: open failed");
        return;
      }
    }
   else // >>
   {
     if ((fd=open(output_file.c_str(),O_CREAT | O_APPEND |O_WRONLY,S_IRWXO | S_IRWXU | S_IRWXG))==-1)
     {
       perror("smash error: open failed");
       return;
     }
   }
   if((old_fd=dup(1))==-1)
   {
    perror("smash error: dup failed");
    if (close(fd) ==-1)
      perror("smash error: close failed");
    return;
   }
   if (close(1) ==-1)
   {
    perror("smash error: close failed");
    if (close(fd) ==-1)
      perror("smash error: close failed");
    if (close(old_fd) ==-1)
      perror("smash error: close failed");
    return;
   }
   if(dup2(fd,1)==-1)
   {
    perror("smash error: dup2 failed");
    if (close(fd) ==-1)
      perror("smash error: close failed");
    if (close(old_fd) ==-1)
      perror("smash error: close failed");
    if(dup2(old_fd,1)==-1)
      perror("smash error: dup2 failed");
    return;
   }
  SmallShell& smash = SmallShell::getInstance();
  Command* cmd = smash.CreateCommand(updated_cmd_line.c_str());
  cmd->execute();
  if (close(1) ==-1)
  {
    perror("smash error: close failed");
    if (close(fd) ==-1)
      perror("smash error: close failed");
    if (close(old_fd) ==-1)
      perror("smash error: close failed");
    return;
  }
  if(dup2(old_fd,1)==-1)
  {
    perror("smash error: dup2 failed");
    if (close(fd) ==-1)
      perror("smash error: close failed");
    if (close(old_fd) ==-1)
      perror("smash error: close failed");
    return;
  }
  if (close(old_fd) ==-1)
  {
    perror("smash error: close failed");
    if (close(fd) ==-1)
      perror("smash error: close failed");
    return;
  }
  if (close(fd) ==-1)
    perror("smash error: close failed");
}


PipeCommand::PipeCommand(const char* cmd_line) : Command(cmd_line), cmd1_line(), cmd2_line()
{
  string curr_arg;
  string fixed_cmd_str=FixCmdPipe(cmd_line);
  const char* fixed_cmd=fixed_cmd_str.c_str();
  char* arg[COMMAND_MAX_ARGS];
  int args_num = _parseCommandLine(fixed_cmd , arg);
  bool first_cmd=true;
  for(int i=0;i<args_num;i++)
  {
    curr_arg = arg[i];
    if(curr_arg.compare("|")==0)
    {
      changedFD=1;
      first_cmd=false;
      i++;
      curr_arg = arg[i];
      cmd2_line+=curr_arg;
      cmd2_line+=" ";
    }
    else if(curr_arg.compare("|&")==0)
    {
      changedFD=2;
      first_cmd=false;
      i++;
      curr_arg = arg[i];
      cmd2_line+=curr_arg;
      cmd2_line+=" ";
    }
    else{
      if(first_cmd)
      {
        cmd1_line+=curr_arg;
        cmd1_line+=" ";
      }
      else
      {
        cmd2_line+=curr_arg;
        cmd2_line+=" ";
      }
    }
  }
  for(int i=0;i<args_num;i++)
    free(arg[i]);
}

std::string FixCmdPipe(const char* cmd)
{
  string old_cmd=cmd;
  size_t pos=old_cmd.find_first_of("|");
  if(pos==std::string::npos)
  {
    return cmd;
  }
  if(old_cmd.at(pos+1)=='&')
    old_cmd.insert(pos+2," ");
  else
    old_cmd.insert(pos+1," ");
  old_cmd.insert(pos," ");
  return old_cmd;
}

void PipeCommand::execute()
{
  pid_t p1;
  if ((p1=fork()) == 0)
  { 
    setpgrp();
    pid_t p2;
    int my_pipe[2];
    pipe(my_pipe); 
    if((p2=fork())==0) //1st command, out changed to pipe[1]
      {
        setpgrp();
        if(close(my_pipe[0])==-1) //close read
        {
          perror("smash error: close failed");
          if(close(my_pipe[1])==-1)
            perror("smash error: close failed");
          exit(0);
        }
        if (close(changedFD) ==-1)
        {//close old out
          perror("smash error: close failed");
          if(close(my_pipe[1])==-1)
            perror("smash error: close failed");
          exit(0);
        }
        if(dup2(my_pipe[1],changedFD)==-1)
        {//change out to new out
          perror("smash error: dup2 failed");
          if(close(my_pipe[1])==-1)
            perror("smash error: close failed");
          exit(0);
        }
        SmallShell& smash = SmallShell::getInstance();
        Command* cmd = smash.CreateCommand(cmd1_line.c_str());
        cmd->execute();
        if (close(changedFD) ==-1)//close the new out
          perror("smash error: close failed");
        if (close(my_pipe[1])==-1)//close write of pipe
          perror("smash error: close failed");
        exit(0);
    }
    else //2nd command, in change to pipe[0]
      {
        if(close(my_pipe[1])==-1) //close read
        {
          perror("smash error: close failed");
          if(close(my_pipe[0])==-1)
            perror("smash error: close failed");
          exit(0);
        }
        if (close(0) ==-1)
        {//close old in
          perror("smash error: close failed");
          if(close(my_pipe[0])==-1)
            perror("smash error: close failed");
          exit(0);
        }
        if(dup2(my_pipe[0],0)==-1)
        {//change in to new in
          perror("smash error: dup2 failed");
          if(close(my_pipe[0])==-1)
            perror("smash error: close failed");
          exit(0);
        }
        SmallShell& smash = SmallShell::getInstance();
        Command* cmd = smash.CreateCommand(cmd2_line.c_str());
        cmd->execute();
        if (close(0) ==-1)//close the new in
          perror("smash error: close failed");
        if (close(my_pipe[0])==-1)//close read of pipe
          perror("smash error: close failed");
        waitpid(p2,NULL,0);
        exit(0);
      }
  }
  else
  {// father, wait for sons to finish
    waitpid(p1,NULL,0);
  } 
}

TimedoutObj::TimedoutObj(std::string command_line,  pid_t pid,  int time_from_now): command_line(command_line),pid(pid) 
{
  time_to_execute=time(0)+time_from_now;
}

bool TimedoutObj::operator<(const TimedoutObj& other) const
{
  return (difftime(other.time_to_execute,time_to_execute)>0);
}


void SmallShell::DeleteCommands()
{
  for (vector<Command*>::iterator it =commands_to_delete.begin(); it!=commands_to_delete.end();)
  {
    delete *it;
    it=commands_to_delete.erase(it);
  }
}