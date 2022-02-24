#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string>
#include <time.h>
#include <set>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (25)

class TimedoutObj{
  public:
  std::string command_line;
  pid_t pid;
  time_t time_to_execute;
  TimedoutObj(std::string command_line, pid_t pid, int time_from_now);
  ~TimedoutObj(){}
  bool operator<(const TimedoutObj& other) const;
};

class Command {
 public:
  std::string original_cmd;
  int jobID;
  Command(const char* cmd_line, bool isBuiltIn=false);
  virtual ~Command() {}
  virtual void execute() = 0;
};

class BuiltInCommand : public Command {
 public:
  BuiltInCommand(const char* cmd_line) ;
  virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
  bool background_command;
  std::string cmd_line;
  bool time_out_cmd;
  int time_of_timeout;
 public:
  void removeTimeout(std::string& cmd_line);
  ExternalCommand(const char* cmd_line);
  virtual ~ExternalCommand() {}
  void execute() override;
};

class PipeCommand : public Command {
 public:
 int changedFD;
 std::string cmd1_line;
 std::string cmd2_line;
  PipeCommand(const char* cmd_line);
  virtual ~PipeCommand() {}
  void execute() override;
};

class RedirectionCommand : public Command {
  private:
 bool overrideOutput;
 std::string updated_cmd_line;
 std::string output_file;
 public:
  RedirectionCommand(const char* cmd_line);
  virtual ~RedirectionCommand() {}
  void execute() override;
};


class changePromptCommand : public BuiltInCommand
{
  private:
  std::string prompt;
  public:
  changePromptCommand(const char* cmd_line);
  virtual ~changePromptCommand() {}
  void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
private:
int args_num;
std::string path;
public:
  ChangeDirCommand(const char* cmd_line);
  virtual ~ChangeDirCommand() {}
  void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
 public:
  GetCurrDirCommand(const char* cmd_line);
  virtual ~GetCurrDirCommand() {}
  void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
 public:
  ShowPidCommand(const char* cmd_line);
  virtual ~ShowPidCommand() {}
  void execute() override;
};

class JobsList;
class QuitCommand : public BuiltInCommand {
  private:
  JobsList* jobs;
  bool kill_flag;
  public:
  QuitCommand(const char* cmd_line, JobsList* jobs);
  virtual ~QuitCommand() {}
  void execute() override;
};

enum status{running_back,stopped};

class JobsList {
 public:
  class JobEntry { //job data
  public:
    int job_id;
    pid_t job_pid;
    time_t inserted_time;
    status job_status;
    std::string cmd_call;
    Command* cmd;
    
   JobEntry(int job_id, pid_t job_pid, time_t inserted_time,
           status job_status, std::string& cmd_call,
            Command* cmd);
   bool operator<(const JobsList::JobEntry other) const;
  };
 std::vector<JobEntry> smash_jobs;

  JobsList() = default;
  ~JobsList() = default;
  void addJob(Command* cmd, pid_t job_pid=-1, bool isStopped=false,bool isForground=false);
  int JobListGetMaxID(bool stopped=false) const;
  void printJobsList();
  void removeFinishedJobs();
  JobEntry* getJobById(int jobId);
  bool JobExistByPid(int jobPid);
  void removeJobById(int jobId);
  void removeJobByPid(pid_t pid);
  JobEntry *getLastStoppedJob(int *jobId);
  void ChangeStatusProcess(int job_id,status new_status);
};

class JobsCommand : public BuiltInCommand {
 private:
  JobsList* jobs;
 public:
  JobsCommand(const char* cmd_line, JobsList* jobs);
  virtual ~JobsCommand() {}
  void execute() override;
};

class KillCommand : public BuiltInCommand {
 private:
 JobsList* jobs;
 bool valid_input;
 int job_id;
 int signal_num;
 public:
  KillCommand(const char* cmd_line, JobsList* jobs);
  virtual ~KillCommand() {}
  void execute() override;
};

enum error_num{no_err,not_exist,list_empty,invalid_arg,stopped_list_empty,already_bg};

class ForegroundCommand : public BuiltInCommand {
 private:
  int job_id;
  error_num err;
  JobsList* jobs;
 public:
  ForegroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~ForegroundCommand() {}
  void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
 private:
  int job_id;
  error_num err;
  JobsList* jobs;
 public:
  BackgroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~BackgroundCommand() {}
  void execute() override;
};

class CatCommand : public BuiltInCommand {
 public:
  char* args[25];
  int args_num;
  CatCommand(const char* cmd_line);
  virtual ~CatCommand() {}
  void execute() override;
};


class SmallShell {
 private:
  const pid_t shell_pid;
  std::string prompt_name;
  std::string former_path;
  SmallShell();
 public:
  pid_t running_pid;
  Command* running_cmd;
  JobsList job_list;
  std::multiset<TimedoutObj> timed_out_set;
  std::vector<Command*> commands_to_delete;
  Command *CreateCommand(const char* input_cmd_line);
  SmallShell(SmallShell const&)      = delete; // disable copy ctor
  void operator=(SmallShell const&)  = delete; // disable = operator
  static SmallShell& getInstance() // make SmallShell singleton
  {
    static SmallShell instance; // Guaranteed to be destroyed.
    // Instantiated on first use.
    return instance;
  }
  ~SmallShell();
  void executeCommand(const char* cmd_line);
  void printPromptName() const;
  void changePromptName(const std::string& new_name);
  pid_t getShellPid() const;
  std::string getFormerPath() const;
  void setFormerPath(const std::string& path);
  void DeleteCommands();
};

//static functions
bool isStringNumber(std::string str);
bool _isTimeoutCommand(std::string& cmd_line);
std::string FixCmdPipe(const char* cmd);
void waitForForegroundProccess(pid_t pid,Command* cmd);

#endif //SMASH_COMMAND_H_
