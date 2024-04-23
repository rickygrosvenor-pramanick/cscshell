/*****************************************************************************/
/*                           CSC209-24s A3 CSCSHELL                          */
/*       Copyright 2024 -- Demetres Kostas PhD (aka Darlene Heliokinde)      */
/*****************************************************************************/

#include "cscshell.h"


// COMPLETE
int cd_cscshell(const char *target_dir){
    if (target_dir == NULL) {
        char user_buff[MAX_USER_BUF];
        if (getlogin_r(user_buff, MAX_USER_BUF) != 0) {
           perror("run_command");
           return -1;
        }
        struct passwd *pw_data = getpwnam((char *)user_buff);
        if (pw_data == NULL) {
           perror("run_command");
           return -1;
        }
        target_dir = pw_data->pw_dir;
    }

    if(chdir(target_dir) < 0){
        perror("cd_cscshell");
        return -1;
    }
    return 0;
}

/*
** Executes a single "line" of commands (through pipes)
** If a command fails, the rest of the line should not be executed.
**
** The error code from the last command is returned through a pointer
** to a heap integer on success. If the line is a `cd` command, the
** return value of `cd_cscshell` is stored by the heap int.
** -- If there are no commands to execute, returns NULL
** -- If there were any errors starting any commands,
**    returns (pointer value) -1
*/
int *execute_line(Command *head){
    int *error_code = malloc(sizeof(int)); // Allocate memory for error code
    *error_code = 0; // Initialize error code to 0

    Command *current_command = head;

    // If there are no commands to execute, returns NULL
    if (head == NULL) {
        free(error_code);
        return NULL;
    }

    // Check for cd
    while (current_command != NULL) {
        if (strcmp(current_command->args[0], "cd") == 0) {
            *error_code = cd_cscshell(current_command->args[1]);
            return error_code;
        }
        current_command = current_command->next;
    }

    current_command = head;

    int command_count = 0;
    while (current_command != NULL) {
        command_count++;
        current_command = current_command->next;
    }

    int child_file_descriptors[command_count - 1][2];
    current_command = head;
    int i = 0;
    while (current_command->next!= NULL) {
        int error = pipe(child_file_descriptors[i]);
        if (error == -1) {
            perror("pipe");
            *error_code = -1;
            return error_code;
        }
        current_command->stdout_fd = child_file_descriptors[i][1];
        current_command->next->stdin_fd = child_file_descriptors[i][0];
        current_command = current_command->next;
        i++;
    }


    pid_t children_pid_arr[command_count];
    current_command = head;
    i = 0;
    while (current_command != NULL) {
        pid_t result = run_command(current_command);
        if (result == -1) {
            return error_code;
        }
        children_pid_arr[i] = result;
        i++;
        current_command = current_command->next;
    }

    for (int i = 0; i < command_count; i++) {
        int status;
        pid_t pid = children_pid_arr[i];
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            if (exit_status != 0) {
                perror("invalid exit status");
                *error_code = exit_status;
            }
        } else {
            perror("Child process terminated abnormally");
            *error_code = -1;
        }
    }
    free_command(current_command);
    return error_code;

    #ifdef DEBUG
    printf("\n***********************\n");
    printf("BEGIN: Executing line...\n");
    #endif

    #ifdef DEBUG
    printf("All children created\n");
    #endif

    // Wait for all the children to finish

    #ifdef DEBUG
    printf("All children finished\n");
    #endif

    #ifdef DEBUG
    printf("END: Executing line...\n");
    printf("***********************\n\n");
    #endif

}


/*
** Forks a new process and execs the command
** making sure all file descriptors are set up correctly.
**
** Parent process returns -1 on error.
** Any child processes should not return.
*/
int run_command(Command *command){
    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        return -1;
    } else if (pid == 0) {
        // Redirect input if needed
        if (command->redir_in_path != NULL) {
            FILE* input_file = fopen(command->redir_in_path, "r");
            if (input_file == NULL) {
                perror("fopen");
                exit(EXIT_FAILURE);
            }
            if (dup2(fileno(input_file), fileno(stdin)) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }

            fclose(input_file);
        } else{
            if (dup2(command->stdin_fd, fileno(stdin)) == -1) {
                perror("dup2");
            }
        }

        if (command->redir_out_path != NULL) {
            // Redirect output if needed
            FILE *output_file;
            if (command->redir_append != 0) {
                output_file = fopen(command->redir_out_path, "a");
            } else {
                output_file = fopen(command->redir_out_path, "w");
            }

            if (output_file == NULL) {
                perror("fopen");
                exit(EXIT_FAILURE);
            }
            if (dup2(fileno(output_file), fileno(stdout)) == -1) {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            fclose(output_file);
        } else {
            if (dup2(command->stdout_fd, fileno(stdout)) == -1) {
                perror("dup2");
            }
        }

        if (command->stdout_fd != fileno(stdout)) {
            close(command->stdout_fd);
        }
        if (command->stdin_fd != fileno(stdin)) {
            close(command->stdin_fd);
        }
        // Execute the command
        if (execv(command->exec_path, command->args) == -1) {
            perror("execv");
            exit(EXIT_FAILURE);
        }
    } else {
        // Parent Process
        if (command->stdout_fd != fileno(stdout)) {
            close(command->stdout_fd);
        }
        if (command->stdin_fd != fileno(stdin)) {
            close(command->stdin_fd);
        }
        return pid;
    }
    return 0;
}

/*
** Executes an entire script line-by-line.
** Stops and indicates an error as soon as any line fails.
**
** Returns 0 on success, -1 on error
*/
int run_script(char *file_path, Variable **root){
    long error;
    char line[MAX_SINGLE_LINE];

    FILE* file = fopen(file_path, "r");

    if (file == NULL) {
        perror("fopen");
        return -1;
    }

    while ((error = (long) fgets(line, MAX_SINGLE_LINE, file)) > 0) {
        // kill the newline
        line[strlen(line) - 1] = '\0';

        Command *commands = parse_line(line, root);
        if (commands == (Command *) -1) {
            ERR_PRINT(ERR_PARSING_LINE);
            continue;
        }
        if (commands == NULL) continue;

        int *last_ret_code_pt = execute_line(commands);
        if (last_ret_code_pt == (int *) -1) {
            ERR_PRINT(ERR_EXECUTE_LINE);
            free(last_ret_code_pt);
            return -1;
        }
        free(last_ret_code_pt);
    }

    if (fclose(file) == EOF) {
        perror("fclose");
        return -1;
    }
    printf("\n");


    return (int) error;
}

void free_command(Command *command){
    Command *curr_command = command;
    while (curr_command != NULL) {
        free(command->exec_path);
        free(command->redir_in_path);
        free(command->redir_out_path);

        int i = 0;
        while (command->args[i] != NULL) {
            free(command->args[i]);
            i++;
        }

        Command* next_command = curr_command->next;
        free(curr_command);
        curr_command = next_command;
    }
}
