#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <pwd.h>
#include <glob.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h> 
#include "variable.h"
#include "struct.h"
#include "myls_myps.h"
using namespace std;

struct shell_info *shell;

struct termios old_termios, new_termios; // use to handel CTRL+D signal

int get_job_id_by_pid(int pid) {
    int i;
    struct process *proc;

    for (i = 1; i <= NR_JOBS; i++) {
        if (shell->jobs[i] != NULL) {
            for (proc = shell->jobs[i]->root; proc != NULL; proc = proc->next) {
                if (proc->pid == pid) {
                    return i;
                }
            }
        }
    }

    return -1;
}

struct job* get_job_by_id(int id) {
    if (id > NR_JOBS) {
        return NULL;
    }

    return shell->jobs[id];
}

int get_proc_count(int id, int filter) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return -1;
    }

    int count = 0;
    struct process *proc;
    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) {
        if (filter == PROC_FILTER_ALL ||
            (filter == PROC_FILTER_DONE && proc->status == STATUS_DONE) ||
            (filter == PROC_FILTER_REMAINING && proc->status != STATUS_DONE)) {
            count++;
        }
    }

    return count;
}

int get_next_job_id() {
    int i;

    for (i = 1; i <= NR_JOBS; i++) {
        if (shell->jobs[i] == NULL) {
            return i;
        }
    }

    return -1;
}

int print_processes_of_job(int id) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return -1;
    }

    cout<<"["<<id<<"]";

    struct process *proc;
    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) {
        cout<<"["<<proc->pid<<"]";
    }
    cout<<endl;
    return 0;
}

int print_job_status(int id) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return -1;
    }
    cout<<"["<<id<<"]";

    struct process *proc;
    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) {
       cout<<"\t"<<proc->pid<<"\t"<< STATUS_STRING[proc->status]<<"\t"<<proc->command;
        if (proc->next != NULL) {
            cout<<"|\n";
        } else {
            cout<<endl;
        }
    }

    return 0;
}

int release_job(int id) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return -1;
    }

    struct job *job = shell->jobs[id];
    struct process *proc, *tmp;
    for (proc = job->root; proc != NULL; ) {
        tmp = proc->next;
        free(proc->command);
        free(proc->argv);
        free(proc->input_path);
        free(proc->output_path);
        free(proc);
        proc = tmp;
    }

    free(job->command);
    free(job);

    return 0;
}

int insert_job(struct job *job) {
    int id = get_next_job_id();

    if (id < 0) {
        return -1;
    }

    job->id = id;
    shell->jobs[id] = job;
    return id;
}

int remove_job(int id) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return -1;
    }

    release_job(id);
    shell->jobs[id] = NULL;

    return 0;
}

int is_job_completed(int id) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return 0;
    }

    struct process *proc;
    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) {
        if (proc->status != STATUS_DONE) {
            return 0;
        }
    }

    return 1;
}

int set_process_status(int pid, int status) {
    int i;
    struct process *proc;

    for (i = 1; i <= NR_JOBS; i++) {
        if (shell->jobs[i] == NULL) {
            continue;
        }
        for (proc = shell->jobs[i]->root; proc != NULL; proc = proc->next) {
            if (proc->pid == pid) {
                proc->status = status;
                return 0;
            }
        }
    }

    return -1;
}

int wait_for_job(int id) {
    if (id > NR_JOBS || shell->jobs[id] == NULL) {
        return -1;
    }

    int proc_count = get_proc_count(id, PROC_FILTER_REMAINING);
    int wait_pid = -1, wait_count = 0;
    int status = 0;

    do {
        wait_pid = waitpid(-shell->jobs[id]->pgid, &status, WUNTRACED);
        wait_count++;

        if (WIFEXITED(status)) {
            set_process_status(wait_pid, STATUS_DONE);
        } else if (WIFSIGNALED(status)) {
            set_process_status(wait_pid, STATUS_TERMINATED);
        } else if (WSTOPSIG(status)) {
            status = -1;
            set_process_status(wait_pid, STATUS_SUSPENDED);
            if (wait_count == proc_count) {
                print_job_status(id);
            }
        }
    } while (wait_count < proc_count);

    return status;
}

int get_command_type(char *command, char ** argv) {
    if (strcmp(command, "exit") == 0) {
        return COMMAND_EXIT;
    } else if (strcmp(command, "cd") == 0) {
        return COMMAND_CD;
    } 
    else if(strcmp(command, "myls") == 0){
        return  COMMAND_MYLS;
    } 
    else if(strcmp(command,"myps") == 0){
        return COMMAND_MYPS;
    }
    else if(strcmp(command,"set") == 0){
        return COMMAND_SET;
    }
    else if((strcmp(command,"echo") == 0 )&& argv[1][0] == '$' ){
        return COMMAND_ECHO;
    }
    else {
        return COMMAND_EXTERNAL;
    }
}

char* helper_strtrim(char* line) {//remove space from the start and end of the string
    char *head = line, *tail = line + strlen(line);

    while (*head == ' ') {
        head++;
    }
    while (*tail == ' ') {
        tail--;
    }
    *(tail + 1) = '\0';

    return head;
}

void mysh_update_cwd_info() {
    getcwd(shell->cur_dir, sizeof(shell->cur_dir));
}

int mysh_cd(int argc, char** argv) {
    if (argc == 1) {
        chdir(shell->pw_dir);
        mysh_update_cwd_info();
        return 0;
    }
    if (chdir(argv[1]) == 0) {
        mysh_update_cwd_info();
        return 0;
    } else {
        cout<<"mysh: cd "<<argv[1]<<"No such file or directory\n";
        return 0;
    }
}

void check_zombie() {
    int status, pid;
    while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED|WCONTINUED)) > 0) {
        if (WIFEXITED(status)) {
            set_process_status(pid, STATUS_DONE);
        } else if (WIFSTOPPED(status)) {
            set_process_status(pid, STATUS_SUSPENDED);
        } else if (WIFCONTINUED(status)) {
            set_process_status(pid, STATUS_CONTINUED);
        }

        int job_id = get_job_id_by_pid(pid);
        if (job_id > 0 && is_job_completed(job_id)) {
            print_job_status(job_id);
            remove_job(job_id);
        }
    }
}


int mysh_execute_builtin_command(struct process *proc) {
    int status = 1;
    switch (proc->type) {
        case COMMAND_EXIT:
            tcsetattr(0,TCSANOW,&old_termios); // reset the signal
            exit(0);
            break;
        case COMMAND_CD:
            mysh_cd(proc->argc, proc->argv);
            break;
        case COMMAND_MYLS:
            myls();
            break;
        case COMMAND_MYPS:
            myps();
            break;
        case COMMAND_SET:
            putenv(proc->argv[1]);
            break;
        case COMMAND_ECHO: 
            char *temp;
            char arr[256];
            int i;
            for(i = 1; proc->argv[1][i]!='\0'; i++){
                arr[i-1] = proc->argv[1][i];
            }
            arr[i-1] = '\0';
            temp= getenv(arr);
            if(temp!=NULL){
                cout<<"\n"<<temp<<"\n";
            }
            break;
        default:
            status = 0;
            break;
    }

    return status;
}

int mysh_launch_process(struct job *job, struct process *proc, int in_fd, int out_fd, int mode) {
    proc->status = STATUS_RUNNING;
    if (proc->type != COMMAND_EXTERNAL && mysh_execute_builtin_command(proc)) {
        return 0;
    }
    
    pid_t childpid;
    int status = 0;

    childpid = fork();

    if (childpid < 0) {
        return -1;
    } else if (childpid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        proc->pid = getpid();
        if (job->pgid > 0) {
            setpgid(0, job->pgid);
        } else {
            job->pgid = proc->pid;
            setpgid(0, job->pgid);
        }

        if (in_fd != 0) {
            dup2(in_fd, 0);
            close(in_fd);
        }

        if (out_fd != 1) {
            dup2(out_fd, 1);
            close(out_fd);
        }

        if (execvp(proc->argv[0], proc->argv) < 0) {
            cout<<"mysh: "<<proc->argv[0]<<": command not found\n";
            exit(0);
        }
        exit(0);
    } else {
        proc->pid = childpid;
        if (job->pgid > 0) {
            setpgid(childpid, job->pgid);
        } else {
            job->pgid = proc->pid;
            setpgid(childpid, job->pgid);
        }

        if (mode == FOREGROUND_EXECUTION) {
            tcsetpgrp(0, job->pgid);
            status = wait_for_job(job->id);
            signal(SIGTTOU, SIG_IGN);
            tcsetpgrp(0, getpid());
            signal(SIGTTOU, SIG_DFL);
        }
    }

    return status;
}

int mysh_launch_job(struct job *job) {
    struct process *proc;
    int status = 0, in_fd = 0, fd[2], job_id = -1;

    check_zombie();
    if (job->root->type == COMMAND_EXTERNAL) {
        job_id = insert_job(job);
    }

    for (proc = job->root; proc != NULL; proc = proc->next) {
        if (proc == job->root && proc->input_path != NULL) {
            in_fd = open(proc->input_path, O_RDONLY);
            if (in_fd < 0) {
                cout<<"mysh: no such file or directory: "<<proc->input_path<<"\n";
                remove_job(job_id);
                return -1;
            }
        }
        if (proc->next != NULL) {
            pipe(fd);
            status = mysh_launch_process(job, proc, in_fd, fd[1], PIPELINE_EXECUTION);
            close(fd[1]);
            in_fd = fd[0];
        } else {
            int out_fd = 1;
            if (proc->output_path != NULL) {
                out_fd = open(proc->output_path, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
                if (out_fd < 0) {
                    out_fd = 1;
                }
            }
            status = mysh_launch_process(job, proc, in_fd, out_fd, job->mode);
        }
    }

    if (job->root->type == COMMAND_EXTERNAL) {
        if (status >= 0 && job->mode == FOREGROUND_EXECUTION) {
            remove_job(job_id);
        } else if (job->mode == BACKGROUND_EXECUTION) {
            print_processes_of_job(job_id);
        }
    }

    return status;
}

struct process* mysh_parse_command_segment(char *segment) {
    int bufsize = TOKEN_BUFSIZE;
    int position = 0;
    char *command = strdup(segment);
    char *token;
    char **Tokens = new char*[bufsize];

    if (!Tokens) {
        fprintf(stderr, "mysh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(segment, TOKEN_DELIMITERS);
    while (token != NULL) {
        glob_t glob_buffer;
        int glob_count = 0;
        if (strchr(token, '*') != NULL || strchr(token, '?') != NULL) {
            glob(token, 0, NULL, &glob_buffer);
            glob_count = glob_buffer.gl_pathc;
        }

        if (position + glob_count >= bufsize) {
            bufsize += TOKEN_BUFSIZE;
            bufsize += glob_count;
            Tokens = (char**) realloc(Tokens, bufsize * sizeof(char*));
            if (!Tokens) {
                fprintf(stderr, "mysh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        if (glob_count > 0) {
            int i;
            for (i = 0; i < glob_count; i++) {
                Tokens[position++] = strdup(glob_buffer.gl_pathv[i]);
            }
            globfree(&glob_buffer);
        } else {
            Tokens[position] = token;
            position++;
        }

        token = strtok(NULL, TOKEN_DELIMITERS);
    }

    int i = 0, argc = 0;
    char *input_path = NULL, *output_path = NULL;
    while (i < position) {
        if (Tokens[i][0] == '<' || Tokens[i][0] == '>') {
            break;
        }
        i++;
    }
    argc = i;

    for (; i < position; i++) {
        if (Tokens[i][0] == '<') {
            if (strlen(Tokens[i]) == 1) {
                input_path = new char[strlen(Tokens[i + 1]) + 1];
                strcpy(input_path, Tokens[i + 1]);
                i++;
            } else {
                input_path = new char[strlen(Tokens[i])];
                strcpy(input_path, Tokens[i] + 1);
            }
        } else if (Tokens[i][0] == '>') {
            if (strlen(Tokens[i]) == 1) {
                output_path = new char[strlen(Tokens[i + 1]) + 1];
                strcpy(output_path, Tokens[i + 1]);
                i++;
            } else {
                output_path = new char [strlen(Tokens[i])];
                strcpy(output_path, Tokens[i] + 1);
            }
        } else {
            break;
        }
    }

    for (i = argc; i <= position; i++) {
        Tokens[i] = NULL;
    }

    struct process *new_proc = new process();
    new_proc->command = command;
    new_proc->argv = Tokens;
    new_proc->argc = argc;
    new_proc->input_path = input_path;
    new_proc->output_path = output_path;
    new_proc->pid = -1;
    new_proc->type = get_command_type(Tokens[0], new_proc->argv);
    new_proc->next = NULL;
    return new_proc;
}

struct job* mysh_parse_command(char *line) {
    line = helper_strtrim(line);
    char *command = strdup(line);

    struct process *root_proc = NULL, *proc = NULL;
    char *line_cursor = line, *c = line, *seg;
    int seg_len = 0, mode = FOREGROUND_EXECUTION;

    if (line[strlen(line) - 1] == '&') {
        mode = BACKGROUND_EXECUTION;
        line[strlen(line) - 1] = '\0';
    }

    while (1) {
        if (*c == '\0' || *c == '|') {
            seg = new char[seg_len+1];
            strncpy(seg, line_cursor, seg_len);
            seg[seg_len] = '\0';

            struct process* new_proc = mysh_parse_command_segment(seg);
            if (!root_proc) {
                root_proc = new_proc;
                proc = root_proc;
            } else {
                proc->next = new_proc;
                proc = new_proc;
            }

            if (*c != '\0') {
                line_cursor = c;
                while (*(++line_cursor) == ' ');
                c = line_cursor;
                seg_len = 0;
                continue;
            } else {
                break;
            }
        } else {
            seg_len++;
            c++;
        }
    }

    struct job *new_job = new job();
    new_job->root = root_proc;
    new_job->command = command;
    new_job->pgid = -1;
    new_job->mode = mode;
    return new_job;
}

char* mysh_read_line() {
    int bufsize = COMMAND_BUFSIZE;
    int position = 0;
    char *buffer = new char[bufsize];
    int c;

    if (!buffer) {
        fprintf(stderr, "mysh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        c = getchar();

        if (c == EOF || c == '\n') {
            buffer[position] = '\0';
            return buffer;
        } else {
            buffer[position] = c;
        }
        position++;

        if (position >= bufsize) {
            bufsize += COMMAND_BUFSIZE;
            buffer = (char *)realloc(buffer, bufsize);
            if (!buffer) {
                fprintf(stderr, "mysh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

void mysh_loop() {
    struct job *job;
    int status = 1;
    while (1) {
        char *line;
        cout<<shell->cur_user<<"$"<<shell->cur_dir<<endl;
        line = mysh_read_line();

        if(line !=NULL){
            if (strlen(line) == 0) {
                check_zombie();
                continue;
            }
            job = mysh_parse_command(line);
            status = mysh_launch_job(job);
        }
    }
}

void SigHandler(int sig){ 
    cout<<"\nCTRL + D \n";
    tcsetattr(0,TCSANOW,&old_termios); // reset the signal
    exit(0);
}

void init_shell() {
    
    setvbuf(stdout,NULL,_IONBF,0);
    tcgetattr(0,&old_termios);

    signal( SIGINT, SigHandler );

    new_termios = old_termios;
    new_termios.c_cc[VINTR] = 4; // over write CTRL + D signal 
    tcsetattr(0,TCSANOW,&new_termios);
    
    pid_t pid = getpid();
    setpgid(pid, pid);
    tcsetpgrp(0, pid);

    shell = new shell_info();
    getlogin_r(shell->cur_user, sizeof(shell->cur_user));

    struct passwd *pw = getpwuid(getuid());
    strcpy(shell->pw_dir, pw->pw_dir);

    int i;
    for (i = 0; i < NR_JOBS; i++) {
        shell->jobs[i] = NULL;
    }

    mysh_update_cwd_info(); // get current directory of the program
}

int main(int argc, char **argv) {

    init_shell();
    mysh_loop();
    return EXIT_SUCCESS;
}
