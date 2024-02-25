/*
Author: @volcaniqueo
Implementation of a simple shell in C.
The specific requirements about the shell can be found in the project description.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/shm.h>

char * temp_last_executed = NULL;  // used to store the last executed command before new command

/*
Function for debugging purposes.
Not related with the shell implementation, just to print args (array of strings type) on the terminal
*/
void print_args(char ** args){
    for(int i = 0; args[i] != NULL; i++){
        printf("%s\n", args[i]);
    }
}

/*
Parser function to parse the input string into an array of strings each seperated by one or more spaces.
The function also modifies necessary variables to handle redirection and background processes.
*/
char ** parser(char *input, char ** outfile, int * mode, int * background){
    char ** args = calloc(64, sizeof(char*));
    char * token;
    int i = 0;

    token = strtok(input, " ");
    while(token != NULL){
        if (strcmp(token, ">") == 0){
            token = strtok(NULL, " ");
            *outfile = token;
            *mode = 1;
            token = strtok(NULL, " ");
            continue;
        }else if (strcmp(token, ">>") == 0){
            token = strtok(NULL, " ");
            *outfile = token;
            *mode = 2;
            token = strtok(NULL, " ");
            continue;
        }else if (strcmp(token, ">>>") == 0){
            token = strtok(NULL, " ");
            *outfile = token;
            *mode = 3;
            token = strtok(NULL, " ");
            continue;
        }
        if (strcmp(token, "&") == 0){
            *background = 1;
            token = strtok(NULL, " ");
            continue;
        }
        args[i] = token;
        i++;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;
    return args;
}

/*
Function to read a line from the terminal.
*/
char * read_line(){
    char * line = NULL;
    size_t bufsize = 0;
    getline(&line, &bufsize, stdin);
    line[strlen(line) - 1] = '\0';
    return line;
}

/*
Function to get the opening prompt of the shell.
Obeys the specific format that is required in the project description.
*/
char * get_opening_prompt(){
    char * username = getlogin();
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    char hostname[1024];
    gethostname(hostname, sizeof(hostname));
    char * prompt = calloc(1024, sizeof(char));
    sprintf(prompt, "%s@%s ~%s --- ", username, hostname, cwd);
    return prompt;
}

/*
Function to get the number of currently running processes.
The "ps" command is run with the help of popen() function.
Then the number of lines in the output of the command is counted.
This function is required for built-in bello() command.
*/
int get_current_procs(){
    FILE * fp = popen("ps", "r");
    if (fp == NULL){
        perror("popen");
        return -1;
    }

    int num_procs = 0;
    char buffer[1024];
    while(fgets(buffer, sizeof(buffer), fp) != NULL){
        num_procs++;
    }
    pclose(fp);
    return num_procs - 1; // Since popen() also creates a process, we subtract 1 from the number of lines.
}

/*
This function is used for the intercommunication between the main process and the child process.
The /myshell_status file is used for this purpose. The file consists of one line with two words.
The first word is the last executed command and the second word is 1 when we have ">>>" type redirection operator with invalid command.
This intercommunication could also be handled with pipes as I did for the special ">>>" redirection operator. But for easiness, and the fact that
the communacation data is always the same in type, I used a file.
*/
char ** read_status(){
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    char status_file[1024];
    strcpy(status_file, cwd);
    strcat(status_file, "/myshell_status");
    FILE * fp;
    fp = fopen(status_file, "r");
    char ** status = calloc(4, sizeof(char*));
    char * line = NULL;
    size_t bufsize = 0;
    getline(&line, &bufsize, fp);
    char * token;
    token = strtok(line, " ");
    status[0] = token;
    token = strtok(NULL, " ");
    status[1] = token;
    return status;
}

/*
The special built-in command bello() is implemented in this function as required in the project description.
The function prints the required information to the stdout.
The information is obtained with the help of the functions in the C library.
Except for the number of currently running processes, which is obtained with the help of the get_current_procs() function.
Please refer to the function's commment section for how the counting is calculated.
*/
void bello(){
    char * username = getlogin();
    char hostname[1024];
    gethostname(hostname, sizeof(hostname));
    char * tty = ttyname(STDIN_FILENO);
    char * shell = getenv("SHELL");
    char * home = getenv("HOME");
    time_t current_time;
    time(&current_time);
    char * time = ctime(&current_time);
    int num_procs = get_current_procs();
    char ** status = read_status();
    printf("Username: %s\n", username);
    printf("Hostname: %s\n", hostname);
    printf("Last executed command: %s\n", status[0]);
    printf("TTY: %s\n", tty);
    printf("Shell: %s\n", shell);
    printf("Home: %s\n", home);
    printf("Time: %s", time);
    printf("Number of processes: %d\n", num_procs);
    fflush(stdout);

}

/*
This function is used to create a new alias.
The aliases are stored in the /myshell_aliases file.
Example saved data: alias volkanik = "echo "buongiorno"" will be saved as volkanik echo "buongiorno"
Please refer to the project description for the syntax of the alias command.
*/
void create_alias(char ** args){
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    char alias_file[1024];
    strcpy(alias_file, cwd);
    strcat(alias_file, "/myshell_aliases");
    FILE * fp;
    fp = fopen(alias_file, "a");
    int i = 0;
    while(args[i] != NULL){
        if(i == 0){
            i++;
            continue;
        }
        if (strcmp(args[i], "=") == 0) {
            i++;
            args[i][0] = ' ';
            continue;
        }
        if (args[i + 1] == NULL){
            args[i][strlen(args[i]) - 1] = '\0';
        }
        fprintf(fp, "%s ", args[i]);
        i++;
    }
    fprintf(fp, "\n");
    fclose(fp);
}

/*
This function is used to check whether the command is an alias.
Example utility:
If we have a line in the /myshell_aliases file as volkanik echo "buongiorno", and then
we type volkanik in the terminal, this function will return echo "buongiorno", and this will be executed
by the execute() function.
*/
char ** get_alias(char ** args){
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    char alias_file[1024];
    strcpy(alias_file, cwd);
    strcat(alias_file, "/myshell_aliases");
    FILE * fp;
    fp = fopen(alias_file, "r");
    char ** alias_converted = calloc(64, sizeof(char*));
    char * line = NULL;
    size_t bufsize = 0;
    while(getline(&line, &bufsize, fp) != -1){
        char * token;
        int i = 0;
        line[strlen(line) - 1] = '\0';
        token = strtok(line, " ");
        if(token == NULL){
            continue;
        }
        if(strcmp(token, args[0]) == 0){
            token = strtok(NULL, " ");
            while(token != NULL){
                alias_converted[i] = token;
                i++;
                token = strtok(NULL, " ");
            }
            int j = 1;
            while(args[j] != NULL){
                alias_converted[i] = args[j];
                i++;
                j++;
            }
            alias_converted[i] = NULL;
            free(args);
            fclose(fp);
            return alias_converted;
        }
    }
    fclose(fp);
    free(alias_converted);
    return args;
}

/*
This function is used to execute the command.
This function exactly equivalent with the C library function execvp().
Since in the project description we are asked to implement searching via PATH, this function
serves for this purpose. If any match occurs, the command is executed with the execv() function.
*/
int execute(char ** args, char * outfile, int mode){

    char * path = getenv("PATH");
    char * path_copy = strdup(path);
    char * dir = strtok(path_copy, ":");
    int result = 0;

    while(dir != NULL){
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "%s/%s", dir, args[0]);

        if (access(cmd, X_OK) == 0) {
            result = execv(cmd, args);
            break;
        }
        dir = strtok(NULL, ":");
    }
    free(path_copy);
    return result;
}

/*
This function is used to override the default SIGINT handler for the main process.
*/
void sigint_handler(int sig){
}

/*
This function is used to write data to the /myshell_status file.
Please refer to the read_status() function's comments for the reasoning about the file as well as its format.
*/
void write_status(char * command, int mode3){
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    char status_file[1024];
    strcpy(status_file, cwd);
    strcat(status_file, "/myshell_status");
    FILE * fp;
    fp = fopen(status_file, "w");
    fprintf(fp, "%s %d", command, mode3);
    fclose(fp);
}

/*
Main function to run the shell.
Before the while loop, the SIGINT handler is set to the sigint_handler() function.
Then the /myshell_aliases file is created if it does not exist.
Then the while loop starts.
The opening prompt is printed to the stdout.
Then the input is read from the terminal.
If the input is empty (has only whitespace(s)), the loop continues.
If the input is "exit", the shell exits.
Then the input is parsed with the help of the parser() function.
The pipe is initialized if the ">>>" operator is used, for intercommunication between the main process and the child process.
If the command is bello, flag is set to avoid execute() function.
If the command is alias, the create_alias() function is called.
Then the command is converted if there is an alias match in the /myshell_aliases file.
Then fork happens, and the child process executes the command with the help of the execute() function.
Prior the execution, the redirection is handled.
If ">>>" operator is used, the parent process reads the data from the pipe and writes it in reversed order to the outfile.
If background operator is used, the parent process does not wait for the child process to finish.
If background is used with ">>>" operator, another fork happens to handle the reversed redirection from the pipe.
*/
int main(){
    signal(SIGINT, sigint_handler);  // Override default SIGINT handler
    write_status("NULL", 0);  // Initialize the /myshell_status file
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    char alias_file[1024];
    strcpy(alias_file, cwd);
    strcat(alias_file, "/myshell_aliases");
    FILE * fp;
    fp = fopen(alias_file, "a");
    fprintf(fp, "\n");
    fclose(fp);
    while(1){

        char * prompt = get_opening_prompt();
        printf("%s", prompt);
        free(prompt);
        char * input;
        input = read_line();
        char * input_copy = strdup(input);

        
        if (strtok(input_copy, " ") == NULL){  // Since strtok() modifies the input, we use a copy of the input
            continue;
        }

        if (strcmp(input, "exit") == 0){
            exit(0);
        }
        
        char * outfile = NULL;
        int background = 0;
        int mode = 0;
        int isbello = 0;
        char ** status = read_status();
        char * last_executed = status[0];
        int mode3_flag = atoi(status[1]);

        char ** args = parser(input, &outfile, &mode, &background);


        int pipefd[2];
        if (mode == 3){
            pipe(pipefd);
        }

        if (strcmp(args[0], "bello") == 0){
            isbello = 1;
        }

        if (strcmp(args[0], "alias") == 0){
            if (background){
                pid_t pid3;
                pid3 = fork();
                if (pid3 < 0){
                    fprintf(stderr, "Fork Failed");
                }
                else if (pid3 == 0){
                    create_alias(args);
                    exit(0);
                }else{
                    continue;
                }
            } else{
                create_alias(args);
                continue;
            }
        }

        
        char ** converted_args = get_alias(args);

        temp_last_executed = last_executed;

        if (isbello == 0){
            write_status(converted_args[0], mode3_flag);
        }

        pid_t pid;
        pid = fork();

        if (pid < 0){
            fprintf(stderr, "Fork Failed");
        }
        else if (pid == 0){
            
            setpgid(0, 0);
            signal(SIGINT, SIG_DFL); // Restore default SIGINT handler for the child process

            if (mode == 1){
                freopen(outfile, "w", stdout);  // ">" operator
            }
            else if (mode == 2){
                freopen(outfile, "a", stdout); // ">>" operator
            }else if (mode == 3){
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO); // ">>>" operator
            }

            int result;
            if (isbello) {
                bello();
                close(pipefd[1]);
            }else{
                result = execute(converted_args, outfile, mode);
            }
            if (result == -1){  // execv() returns -1 if there is an error
                printf("%i\n", result);
                printf("MyShellError: %s \n", strerror(errno));
            }else if (isbello == 0 && result == 0){  // Command is not found in the PATH
                printf("MyShellError: No such command\n");
                if (mode == 3){
                    write_status(last_executed, 1);
                }
            }
            if (isbello == 0){
                write_status(temp_last_executed, mode3_flag);
            }
            exit(0);
        }
        else{
            if (background == 0){
                wait(NULL);
            }
            char ** status = read_status();
            mode3_flag = atoi(status[1]);
            if (mode == 3 && mode3_flag == 0){
                if (background == 0){
                    close(pipefd[1]);
                    char buffer[1024];
                    ssize_t len = read(pipefd[0], buffer, sizeof(buffer));
                    buffer[len - 1] = '\0';
                    len -= 1;
                    /*
                    Reverse the data in the pipe.
                    */
                    for(int i = 0; i < len / 2; i++){
                        char temp = buffer[i];
                        buffer[i] = buffer[len - i - 1];
                        buffer[len - i - 1] = temp;
                    }
                    len += 1;
                    buffer[len - 1] = '\n';
                    buffer[len] = '\0';

                    FILE *fp = fopen(outfile, "a");
                    fputs(buffer, fp);
                    fclose(fp);
                }else{
                    pid_t pid2;
                    pid2 = fork();
                    if (pid2 < 0){
                        fprintf(stderr, "Fork Failed");
                    }
                    else if (pid2 == 0){
                        wait(NULL);
                        close(pipefd[1]);
                        char buffer[1024];
                        ssize_t len = read(pipefd[0], buffer, sizeof(buffer));
                        buffer[len - 1] = '\0';
                        len -= 1;
                        /*
                        Reverse the data in the pipe.
                        */
                        for(int i = 0; i < len / 2; i++){
                            char temp = buffer[i];
                            buffer[i] = buffer[len - i - 1];
                            buffer[len - i - 1] = temp;
                        }
                        len += 1;
                        buffer[len - 1] = '\n';
                        buffer[len] = '\0';

                        FILE *fp = fopen(outfile, "a");
                        fputs(buffer, fp);
                        fclose(fp);
                        exit(0);
                    }
                }
            }
        }
    }
}




    