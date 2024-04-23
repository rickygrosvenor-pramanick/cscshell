/*****************************************************************************/
/*                           CSC209-24s A3 CSCSHELL                          */
/*       Copyright 2024 -- Demetres Kostas PhD (aka Darlene Heliokinde)      */
/*****************************************************************************/

#include "cscshell.h"
#include <ctype.h>

#define CONTINUE_SEARCH NULL

// TODO: ADD ERRORS FOR FAILING MALLOCS ETC.
// TODO: FIX SEG FAULTS

// COMPLETE
char *resolve_executable(const char *command_name, Variable *path){

    if (command_name == NULL || path == NULL) {
        return NULL;
    }

    if (strcmp(command_name, CD) == 0){
        return strdup(CD);
    }

    if (strcmp(path->name, PATH_VAR_NAME) != 0){
        ERR_PRINT(ERR_NOT_PATH);
        return NULL;
    }

    char *exec_path = NULL;

    if (strchr(command_name, '/')){
        exec_path = strdup(command_name);
        if (exec_path == NULL){
            perror("resolve_executable");
            return NULL;
        }
        return exec_path;
    }

    // we create a duplicate so that we can mess it up with strtok
    char *path_to_toke = strdup(path->value);
    if (path_to_toke == NULL){
        perror("resolve_executable");
        return NULL;
    }
    char *current_path = strtok(path_to_toke, ":");

    do {
        DIR *dir = opendir(current_path);
        if (dir == NULL){
            ERR_PRINT(ERR_BAD_PATH, current_path);
            closedir(dir);
            continue;
        }

        struct dirent *possible_file;

        while (exec_path == NULL) {
            // rare case where we should do this -- see: man readdir
            errno = 0;
            possible_file = readdir(dir);
            if (possible_file == NULL) {
                if (errno > 0){
                    perror("resolve_executable");
                    closedir(dir);
                    goto res_ex_cleanup;
                }
                // end of files, break
                break;
            }

            if (strcmp(possible_file->d_name, command_name) == 0){
                // +1 null term, +1 possible missing '/'
                size_t buflen = strlen(current_path) +
                                strlen(command_name) + 1 + 1;
                exec_path = (char *) malloc(buflen);
                // also sets remaining buf to 0
                strncpy(exec_path, current_path, buflen);
                if (current_path[strlen(current_path)-1] != '/'){
                    strncat(exec_path, "/", 2);
                }
                strncat(exec_path, command_name, strlen(command_name)+1);
            }
        }
        closedir(dir);

        // if this isn't null, stop checking paths
        if (possible_file) break;

    } while ((current_path = strtok(CONTINUE_SEARCH, ":")));

    res_ex_cleanup:
    free(path_to_toke);
    return exec_path;
}


// Whitespace trimmer functions
/**
 * Trim leading whitespace characters from a string pointer.
 *
 * This function modifies the pointer to the string by advancing it past
 * any leading whitespace characters (spaces, tabs, etc.).
 *
 * @param line Pointer to the pointer to the string.
 *             The pointer will be modified to point to the first non-whitespace character.
 */
void trim_whitespace_leading(char **line) {
    while (**line == ' ') {
        (*line)++;
    }
}

/**
 * Trim trailing whitespace characters from the end of a string.
 *
 * This function modifies the input string by removing any whitespace characters
 * (spaces, tabs, etc.) from the end of the string until a non-whitespace character
 * is encountered.
 *
 * @param line The string to trim trailing whitespace from.
 */
void trim_whitespace_ending(char *line) {
    int i = strlen(line) - 1;
    while (line[i] == ' ') {
        i--;
    }
    line[i + 1] = '\0';
}


// HELPERS FOR VARIABLE ASSIGNMENT
/**
 * Add or update a variable in a linked list of variables.
 *
 * If the variable with the given name already exists in the list, its value will be updated.
 * If the variable does not exist, a new variable will be added to the list of variables **variables.
 *
 * @param name The name of the variable to add or update.
 * @param value The value of the variable.
 * @param variables Pointer to a pointer to the head of the linked list of variables.
 *                  This pointer will be updated if a new variable is added to the list.
 */
void add_variable(const char *name, const char *value, Variable **variables) {
    // IF we encounter a variable name which already exists
    Variable *curr_var = variables[0];
    while (curr_var != NULL) {
        if (strcmp(name, curr_var->name) == 0) {
            free(curr_var->value);
            curr_var->value = strdup(value);
            return;
        }
        curr_var = curr_var->next;
    }

    if (variables[0] == NULL) {
        // IF the linked list is Uninitialised
        Variable *new_variable = malloc(sizeof(Variable));
        new_variable->name = strdup(name);
        new_variable->value = strdup(value);
        new_variable->next = NULL;
        *variables = new_variable;
    } else if (strcmp(name, PATH_VAR_NAME) == 0 && (variables[0] != NULL && strcmp(variables[0]->name, PATH_VAR_NAME) != 0)){
        // If variable name is PATH or (first variable is not Null AND it is not PATH)
        struct Variable *new_var = malloc(sizeof(struct Variable));
        new_var->name = strdup(name);
        new_var->value = strdup(value);
        new_var->next = *variables;
        *variables = new_var;
    } else if (strcmp(name, PATH_VAR_NAME) != 0 && (variables[0] != NULL && strcmp(variables[0]->name, PATH_VAR_NAME) == 0)) {
        // IF the variable name is not PATH and PATH has Already been assigned
        Variable *curr_path_variable = variables[0];
        Variable *old_next = curr_path_variable->next;

        Variable *new_var = malloc(sizeof(Variable));
        new_var->name = strdup(name);
        new_var->value = strdup(value);
        new_var->next = old_next;
        curr_path_variable->next = new_var;
    } else if (variables[0] != NULL) {
        // IF the first variable is not PATH
        Variable *new_var = malloc(sizeof(Variable));
        new_var->name = strdup(name);
        new_var->value = strdup(value);
        new_var->next = *variables;
        *variables = new_var;
    }
}

// HELPERS FOR COMMANDS
/**
 * Return the number of pipe characters '|' in the given string.
 *
 * @param line The string to search for pipe characters.
 * @return The number of pipe characters '|' found in the string.
 */
int num_pipes(const char *line) {
    int length = strlen(line);
    int num_pipes = 0;

    for (int i = 0; i < length; i++) {
        if (line[i] == '|') {
            num_pipes++;
        }
    }
    return num_pipes;
}

/**
 * Split a command line into separate subcommands separated by pipe ('|') characters.
 *
 * This function parses a command line `line`, separating it into individual subcommands
 * based on the number of pipe characters (`|`) present. It allocates memory dynamically
 * for an array of strings to store each subcommand.
 *
 * @param line The line to be split into subcommands.
 * @param num_pipes The number of pipe characters ('|') in the command line, which indicates
 *                  the number of subcommands.
 * @return An array of strings, each containing a separate subcommand from the command line.
 *         The caller is responsible for freeing the memory allocated for the array and its elements.
 */
char **return_pipe_subcommands(char *line, int num_pipes) {
    int num_subcommands = num_pipes + 1;
    char **subcommands = malloc( num_subcommands * sizeof(char *));
    for (int i = 0; i < num_subcommands; i++) {
        subcommands[i] = malloc(MAX_SINGLE_LINE * sizeof(char));
    }

    char *line_copy = strdup(line);
    char *line_begin = line_copy;
    int i = 0;
    int length;
    while((line_copy = strchr(line_copy, '|')) != NULL && strlen(line_copy) > 1) {
        length = line_copy - line_begin;
        char subcommand[length + 1];
        memcpy(subcommand, line_begin, length);
        subcommand[length] = '\0';

        char *subcommand_ptr = malloc(sizeof(char) * (length + 1));
        char *subcommand_ptr_beginning = subcommand_ptr;
        memset(subcommand_ptr, '\0', sizeof(char) * (length + 1));
        strcpy(subcommand_ptr, subcommand);
        trim_whitespace_leading(&subcommand_ptr);
        trim_whitespace_ending(subcommand_ptr);

        strcpy(subcommands[i], subcommand_ptr);

        free(subcommand_ptr_beginning);
        i++;
        line_copy++;
        line_begin = line_copy;
    }

    length = strlen(line_begin);
    char subcommand[length + 1];
    memcpy(subcommand, line_begin, length);
    subcommand[length] = '\0';

    char *subcommand_ptr = malloc(sizeof(char) * (length + 1));
    char *subcommand_ptr_beginning = subcommand_ptr;
    memset(subcommand_ptr, '\0', sizeof(char) * (length + 1));
    strcpy(subcommand_ptr, subcommand);
    trim_whitespace_leading(&subcommand_ptr);
    trim_whitespace_ending(subcommand_ptr);

    strcpy(subcommands[i], subcommand_ptr);
    free(subcommand_ptr_beginning);
    return subcommands;
}

typedef struct RedirectionCommand {
    char *command_with_args;
    char *redir_in_path;
    char *redir_out_path;
    uint8_t redir_append;
}RedirectionCommand;

/**
 * Parse a pipe subcommand string and return a RedirectionCommand struct.
 *
 * This function parses a pipe subcommand string to extract command with arguments,
 * input and output redirection paths, and append flag if present.
 *
 * @param pipe_subcommand The pipe subcommand string to parse.
 * @return A pointer to a RedirectionCommand structure containing parsed information.
 *         Memory is allocated dynamically for the structure and its members.
 *         Returns NULL if memory allocation fails or if the input is invalid.
 */
RedirectionCommand *return_redirection_command(char *pipe_subcommand) {
    char *pipe_subcommand_copy = strdup(pipe_subcommand);
    RedirectionCommand *command = malloc(sizeof(RedirectionCommand));

    if (strchr(pipe_subcommand_copy, '>') == NULL && strchr(pipe_subcommand_copy, '<') == NULL) {
        command->command_with_args = strdup(pipe_subcommand_copy);
        command->redir_in_path = NULL;
        command->redir_out_path = NULL;
        command->redir_append = 0;
    } else if (strchr(pipe_subcommand_copy, '<') == NULL && strchr(pipe_subcommand_copy, '>') != NULL) {
        char *redir_out_ptr = strchr(pipe_subcommand_copy, '>');
        if (*(redir_out_ptr + 1) == '>') {
            // Append to OUT Path Redirection

            // Handling Command with Args
            char *redir_append = strstr(pipe_subcommand_copy, ">>");
            int length_command_with_args = redir_append - pipe_subcommand_copy;
            char *command_with_args = malloc(sizeof(char) * (length_command_with_args + 1));
            char *command_with_args_beginning = command_with_args;
            memcpy(command_with_args, pipe_subcommand_copy, length_command_with_args);
            command_with_args[length_command_with_args] = '\0';
            trim_whitespace_ending(command_with_args);
            redir_append += 2;
            command->command_with_args = strdup(command_with_args);
            free(command_with_args_beginning);

            // Handling Path
            char *redir_out_path = malloc(sizeof(char) * (strlen(redir_append) + 1));
            char *original_redir_out_path = redir_out_path;
            int length = strlen(redir_append);
            memcpy(redir_out_path, redir_append, length);
            redir_out_path[length] = '\0';
            trim_whitespace_leading(&redir_out_path);
            command->redir_out_path = strdup(redir_out_path);
            free(original_redir_out_path);

            // Complete Command
            command->redir_in_path = NULL;
            command->redir_append = 1;
        } else {
            // Write OUT to New Path Redirection

            // Handling Command With Args
            int length_command_with_args = redir_out_ptr - pipe_subcommand_copy;
            char *command_with_args = malloc(sizeof(char) * (length_command_with_args + 1));
            char *command_with_args_beginning = command_with_args;
            memcpy(command_with_args, pipe_subcommand_copy, length_command_with_args);
            command_with_args[length_command_with_args] = '\0';
            trim_whitespace_ending(command_with_args);
            redir_out_ptr++;
            command->command_with_args = strdup(command_with_args);
            free(command_with_args_beginning);

            // Handling Path
            char *redir_out_path = malloc(sizeof(char) * (strlen(redir_out_ptr) + 1));
            char *redir_out_path_beginning = redir_out_path;
            int length = strlen(redir_out_ptr);
            memcpy(redir_out_path, redir_out_ptr, length);
            redir_out_path[length] = '\0';
            trim_whitespace_leading(&redir_out_path);
            command->redir_out_path = strdup(redir_out_path);
            free(redir_out_path_beginning);

            // Complete Command
            command->redir_in_path = NULL;
            command->redir_append = 0;
        }
    } else if (strchr(pipe_subcommand_copy, '<') != NULL && strchr(pipe_subcommand_copy, '>') == NULL) {
        char *redir_in_ptr = strchr(pipe_subcommand_copy, '<');
        // Read IN From Path

        // Handling Command With Args
        int length_command_with_args = redir_in_ptr - pipe_subcommand_copy;
        char *command_with_args = malloc(sizeof(char) * (length_command_with_args + 1));
        char *command_with_args_beginning = command_with_args;
        memcpy(command_with_args, pipe_subcommand_copy, length_command_with_args);
        command_with_args[length_command_with_args] = '\0';
        trim_whitespace_ending(command_with_args);
        redir_in_ptr++;
        command->command_with_args = strdup(command_with_args);
        free(command_with_args_beginning);

        // Handling Path
        char *redir_in_path = malloc(sizeof(char) * (strlen(redir_in_ptr) + 1));
        char *redir_in_path_beginning = redir_in_path;
        int length = strlen(redir_in_ptr);
        memcpy(redir_in_path, redir_in_ptr, length);
        redir_in_path[length] = '\0';
        trim_whitespace_leading(&redir_in_path);
        command->redir_in_path = strdup(redir_in_path);
        free(redir_in_path_beginning);

        // Complete Command
        command->redir_out_path = NULL;
        command->redir_append = 0;
    } else {
        int append_out_flag = 0;
        if (strstr(pipe_subcommand_copy, ">>") != NULL) {
            // Accounts for appending out redirection + inward redirection
            append_out_flag = 1;
        }

        char *redir_out_ptr;
        if (append_out_flag == 1) {
            redir_out_ptr = strstr(pipe_subcommand_copy, ">>");
            command->redir_append = 1;
        } else {
            // append_out_flag == 0
            redir_out_ptr = strchr(pipe_subcommand_copy, '>');
            command->redir_append = 0;
        }

        char *redir_in_ptr = strchr(pipe_subcommand_copy, '<');

        if (redir_in_ptr < redir_out_ptr) {
            // First Split on redir_in_ptr (Handling Command With Args)
            int length_command_with_args = redir_in_ptr - pipe_subcommand_copy;
            char *command_with_args = malloc(sizeof(char) * (length_command_with_args + 1));
            char *command_with_args_beginning = command_with_args;
            memcpy(command_with_args, pipe_subcommand_copy, length_command_with_args);
            command_with_args[length_command_with_args] = '\0';
            trim_whitespace_ending(command_with_args);
            redir_in_ptr++;
            command->command_with_args = strdup(command_with_args);
            free(command_with_args_beginning);

            // Handling remaining command
            char *redir_in_path = malloc(sizeof(char) * (strlen(redir_in_ptr) + 1));
            char *redir_in_path_beginning = redir_in_path;
            int length = strlen(redir_in_ptr);
            memcpy(redir_in_path, redir_in_ptr, length);
            redir_in_path[length] = '\0';
            trim_whitespace_leading(&redir_in_path);

            // Second Split on redir_out_ptr (Handling redir_in_path)
            char *redir_out_path_in_substr;
            if (append_out_flag == 1) {
                redir_out_path_in_substr = strstr(redir_in_path, ">>");
                command->redir_append = 1;
            } else {
                redir_out_path_in_substr = strchr(redir_in_path, '>');
                command->redir_append = 0;
            }

            length = redir_out_path_in_substr - redir_in_path;
            char *command_with_args_in_path = malloc(sizeof(char) * (length + 1));
            char *command_with_args_in_path_beginning = command_with_args_in_path;
            memcpy(command_with_args_in_path, redir_in_path, length);
            command_with_args_in_path[length] = '\0';
            trim_whitespace_ending(command_with_args_in_path);
            command->redir_in_path = strdup(command_with_args_in_path);
            free(command_with_args_in_path_beginning);

            if (append_out_flag == 1) {
                redir_out_path_in_substr += 2;
            } else {
                redir_out_path_in_substr++;
            }

            // Path of the second Split (Handling redir_out_path)
            length = strlen(redir_out_path_in_substr);
            char *redir_out_path = malloc(sizeof(char) * (length + 1));
            char *redir_out_path_beginning = redir_out_path;
            memcpy(redir_out_path, redir_out_path_in_substr, length);
            redir_out_path[length] = '\0';
            trim_whitespace_leading(&redir_out_path);
            command->redir_out_path = strdup(redir_out_path);
            free(redir_out_path_beginning);
            free(redir_in_path_beginning);
        } else {
            // First Split on redir_out_ptr (Handling Command With Args)
            int length_command_with_args = redir_out_ptr - pipe_subcommand_copy;
            char *command_with_args = malloc(sizeof(char) * (length_command_with_args + 1));
            char *command_with_args_beginning = command_with_args;
            memcpy(command_with_args, pipe_subcommand_copy, length_command_with_args);
            command_with_args[length_command_with_args] = '\0';
            trim_whitespace_ending(command_with_args);
            if (append_out_flag == 1) {
                redir_out_ptr += 2;
                command->redir_append = 1;
            } else {
                redir_out_ptr++;
                command->redir_append = 0;
            }
            command->command_with_args = strdup(command_with_args);
            free(command_with_args_beginning);

            // Handling remaining command
            char *redir_out_path = malloc(sizeof(char) * (strlen(redir_out_ptr) + 1));
            char *redir_out_path_beginning = redir_out_path;
            int length = strlen(redir_out_ptr);
            memcpy(redir_out_path, redir_out_ptr, length);
            redir_out_path[length] = '\0';
            trim_whitespace_leading(&redir_out_path);

            // Second Split on redir_in_ptr (Handling redir_out_path)
            char *redir_in_path_in_substr = strchr(redir_out_path, '<');
            length = redir_in_path_in_substr - redir_out_path;
            char *command_with_args_in_path = malloc(sizeof(char) * (length + 1));
            char *command_with_args_in_path_beginning = command_with_args_in_path;
            memcpy(command_with_args_in_path, redir_out_path, length);
            command_with_args_in_path[length] = '\0';
            trim_whitespace_ending(command_with_args_in_path);
            command->redir_out_path = strdup(command_with_args_in_path);
            free(command_with_args_in_path_beginning);
            redir_in_path_in_substr++;

            // Path of the second Split (Handling redir_in_path)
            length = strlen(redir_in_path_in_substr);
            char *redir_in_path = malloc(sizeof(char) * (length + 1));
            char *redir_in_path_beginning = redir_in_path;
            memcpy(redir_in_path, redir_in_path_in_substr, length);
            redir_in_path[length] = '\0';
            trim_whitespace_leading(&redir_in_path);
            command->redir_in_path = strdup(redir_in_path);
            free(redir_in_path_beginning);
            free(redir_out_path_beginning);
        }
    }

    return command;
}

/**
 * Separate a command_with_arguments string into its executable path and arguments.
 *
 * This function parses a command_with_arguments string to extract its executable path
 * and arguments. It also resolves the executable path using the provided
 * environment variable path if necessary.
 *
 * @param commands_with_args The command string containing the executable path and arguments.
 * @param path Pointer to the head of the linked list of environment variables representing the PATH.
 * @return A pointer to a Command structure containing the parsed information.
 *         Memory is allocated dynamically for the structure and its members.
 *         Returns NULL if memory allocation fails or if the executable path cannot be resolved.
 */
Command* separate_command_and_args(char* commands_with_args, Variable* path) {
    Command *command = malloc(sizeof(Command));

    // Handling the command's exec_path
    char *space_ptr = strchr(commands_with_args, ' ');
    if (space_ptr == NULL) {
        // Command takes no arguments
        command->exec_path = resolve_executable(commands_with_args, path);

        if (command->exec_path == NULL) {
            free(command);
            ERR_PRINT(ERR_NO_EXECU, commands_with_args);
            return NULL;
        }
        command->args = malloc(sizeof(char *) * 2);
        command->args[0] = strdup(commands_with_args);
        command->args[1] = NULL;
        return command;
    }

    int command_name_len = space_ptr - commands_with_args;
    char command_name[command_name_len + 1];
    memcpy(command_name, commands_with_args, command_name_len);
    command_name[command_name_len] = '\0';
    command->exec_path = resolve_executable(command_name, path);
    if (command->exec_path == NULL) {
        free(command);
        ERR_PRINT(ERR_NO_EXECU, command_name);
        return NULL;
    }
    commands_with_args += command_name_len + 1;


    // Handling the command's arguments
    int args_count = 0;
    for (int i = 0; i < strlen(commands_with_args); i++) {
        if (commands_with_args[i] == ' ') {
            args_count++;
        }
    }
    // Number of arguments is 1 more than the number of spaces and the name of the executable
    // should also be in the args
    args_count += 2;
    char **args = malloc(sizeof(char*) * args_count);
    args[0] = strdup(command_name);
    int i = 1;
    while ((space_ptr = strchr(commands_with_args, ' ')) != NULL) {
        int length = space_ptr - commands_with_args;
        args[i] = malloc(sizeof(char) * (length + 1));
        memcpy(args[i], commands_with_args, length);
        args[i][length] = '\0';
        commands_with_args+= length + 1;
        i++;
    }
    int length_last_arg = strlen(commands_with_args);
    args[i] = commands_with_args;
    args[i][length_last_arg] = '\0';
    args[i + 1] = NULL;
    command->args = args;
    return command;
}

/*
** Parses a single line of text and returns a linked list of commands.
** The last command in the list has a next pointer that points to NULL.
**
** Return possibilities:
** 1. The first in a list of commands that should execute roughly
**    simultaneously (see instructions for details).
**
** 2. NULL if successfully parsed line, with *no commands*, this happens:
**      -- Case 1: Empty line
**      -- Case 2: Line is /exclusively/ a comment (i.e. first non-whitespace
**                 char is '#'). Comments may also trail commands or assignments.
**                 You must handle text before "#' characters.
**      -- Case 3: Shell variable assignment (e.g. VAR=VALUE)
**          -- The variable should added to the variables list
**          -- or updated if the variable already exists
**
** 3. If there is an error, returns -1 cast as a (Command *)
 */
Command *parse_line(char *line, Variable **variables){
    // Check for empty line or comment
    if (line == NULL) {
        return (Command *) -1;
    }
    trim_whitespace_leading(&line);
    trim_whitespace_ending(line);
    if (*line == '\0' || *line == '#' || strlen(line) == 0) {
        return NULL;
    }

    line = replace_variables_mk_line(line, *variables);

    // Check for variable assignment
    if (strchr(line, '=')) {
        // Parse variable assignment
        const char *name = strtok(line, "=");
        const char *value = strtok(NULL, "#");
        if (name == NULL) {
            ERR_PRINT(ERR_VAR_START);
            return (Command *) -1;
        }

        for (int i = 0; i < strlen(name); i++) {
            if (name[i] == '#') {
                return NULL;
            }
            if (isalpha(name[i]) == 0 && name[i] != '_') {
                // Handles "Shouldn't have a space before the equal sign"
                // Handles Shell variables must be specified as a name, consisting of only alphabetic
                // characters and _ (underscore) characters.
                ERR_PRINT(ERR_VAR_NAME, name);
                return (Command *) -1;
            }
        }
        add_variable(name, value, variables);
        return NULL;
    } else {
        int pipe_count = num_pipes(line);
        char **pipe_subcommands = return_pipe_subcommands(line, pipe_count);
        int subcommand_count = pipe_count + 1;

        Command *subcommands[subcommand_count];

        for (int i = 0; i < subcommand_count; i++) {
            RedirectionCommand *redir_command = return_redirection_command(pipe_subcommands[i]);
            Command* command = separate_command_and_args(redir_command->command_with_args, *variables);
            if (command == NULL) {
                return NULL;
            }
            free(redir_command->command_with_args);
            command->redir_in_path = redir_command->redir_in_path;
            command->redir_out_path = redir_command->redir_out_path;
            command->redir_append = redir_command->redir_append;
            command->stdin_fd = STDIN_FILENO;
            command->stdout_fd = STDOUT_FILENO;
            command->next = NULL;


            free(redir_command);
            subcommands[i] = command;
        }

        for (int i = 0; i < subcommand_count; i++) {
            free(pipe_subcommands[i]);
        }

        free(pipe_subcommands);

        for (int i = 0; i < subcommand_count - 1; i++) {
            subcommands[i]->next = subcommands[i + 1];
        }

        Command *curr_command = subcommands[0];
        while (curr_command != NULL) {
            curr_command = curr_command->next;
        }

        return subcommands[0];
    }

}

// HELPERS FOR replace_variables_mk_line
/**
 * Get the number of variables in a line.
 *
 * This function counts the number of variables ('$' symbols) present in a line.
 *
 * @param line The line containing variables.
 * @return The number of variables in the line.
 */
int get_num_variables(const char *line) {
    int count = 0;
    for (int i = 0; i < strlen(line); i++) {
        if (line[i] == '$') {
            count++;
        }
    }
    return count;
}

typedef struct RemovedVariables {
    char **var_names;
    int removed;
    int num_removed;
} RemovedVariables;

/**
 * Extract variable names, number of characters removed, and the number of variables removed from a line.
 *
 * This function extracts variable names from a line containing variables ('$' symbols).
 *
 * @param line The line containing variables.
 * @return A pointer to a RemovedVariables structure containing the extracted variable names and other information.
 *         Memory is allocated dynamically for the structure and its members.
 */
RemovedVariables *variable_names(const char *line) {
    int num_vars = get_num_variables(line);
    char **var_names = malloc(num_vars * sizeof(char *));
    for (int i = 0; i < num_vars; i++) {
        var_names[i] = malloc(sizeof(char) * MAX_SINGLE_LINE);
    }

    char *line_copy = strdup(line);
    char *line_begin = line_copy;
    int i = 0;
    int removed = 0;

    while ((line_copy = strchr(line_copy, '$')) != NULL && strlen(line_copy) > 1) {
        line_copy++;
        removed++;
        if (*line_copy == '{') {
            char *right_curl_position = strchr(line_copy, '}');
            int length =  right_curl_position - 1 - (line_copy + 1) + 1;
            char name_var[length + 1];
            removed += length + 2;
            memcpy(name_var, line_copy + 1, length);
            name_var[length] = '\0';
            strcpy(var_names[i], name_var);
            i++;
        } else {
            int length;
            char *next_space = strchr(line_copy, ' ');
            char *next_dollar = strchr(line_copy, '$');
            char *next_ptr;

            if (next_space == NULL && next_dollar == NULL) {
                // WORKS FOR $ABC
                length = strlen(line_begin) - (line_copy - line_begin);

            } else if (next_dollar != NULL && (next_space == NULL || next_dollar < next_space)) {
                // WORKS FOR CASES $ABC$XYZ
                next_ptr = next_dollar;
                length = next_ptr - line_copy;
            } else {
                // (next_dollar == NULL || next_space < next_dollar)
                // WORK FOR $ABC $XYZ
                next_ptr = next_space;
                length = next_ptr - line_copy;
            }
            char name_var[length + 1];
            removed += length;
            memcpy(name_var, line_copy, length);
            name_var[length] = '\0';
            strcpy(var_names[i], name_var);
            i++;
        }
    }
    RemovedVariables *removedVariablesToReturn = malloc(sizeof(RemovedVariables));
    removedVariablesToReturn->var_names = malloc(num_vars);
    for (int i = 0; i < num_vars; i++) {
        removedVariablesToReturn->var_names[i] = malloc(sizeof(char) * (strlen(var_names[i]) + 1));
        strcpy(removedVariablesToReturn->var_names[i], var_names[i]);
    }
    removedVariablesToReturn->removed = removed;
    removedVariablesToReturn->num_removed = num_vars;

    for (int i = 0; i < num_vars; i++) {
        free(var_names[i]);
    }
    free(var_names);

    return removedVariablesToReturn;
}

/**
 * Retrieve values corresponding to removed variables from the list of variables.
 *
 * This function retrieves the values corresponding to removed variables from the list of variables.
 * For each removed variable name, it searches the linked list of variables and copies the value
 * of the variable to the corresponding position in the returned array.
 *
 * @param removed_variables Pointer to a RemovedVariables structure containing removed variable names.
 * @param variables Pointer to the head of the linked list of variables.
 * @return An array of strings containing the values corresponding to the removed variables.
 *         Memory is allocated dynamically for the array and its elements.
 *         Returns NULL for variables whose values cannot be found.
 */
char **values_from_removed_variables(RemovedVariables *removed_variables, Variable *variables) {
    char **values_from_removed_variables = malloc(removed_variables->num_removed * sizeof(char *));
    int num_removed = removed_variables->num_removed;
    for (int i = 0; i < num_removed; i++) {
        values_from_removed_variables[i] = malloc(sizeof(char) * MAX_SINGLE_LINE);
        memset(values_from_removed_variables[i], '\0', sizeof(char) * MAX_SINGLE_LINE);
        int var_not_exit_flag = 0;
        Variable *curr = variables;
        while (curr != NULL) {
            if (strcmp(curr->name, removed_variables->var_names[i]) == 0) {
                strncpy(values_from_removed_variables[i], curr->value, strlen(curr->value));
                values_from_removed_variables[i][strlen(curr->value)] = '\0';
                var_not_exit_flag = 1;
            }
            curr = curr->next;
        }
        if (var_not_exit_flag == 0) {
            ERR_PRINT(ERR_VAR_NOT_FOUND, removed_variables->var_names[i]);
        }
    }

    return values_from_removed_variables;
}

/**
 * Create a new line with substituted variable values.
 *
 * This function creates a new line by substituting the values of removed variables with the provided values.
 * It calculates the size of the new line, allocates memory for it, and performs the substitution.
 *
 * @param removed_variables Pointer to a RemovedVariables structure containing removed variable names.
 * @param line The original line containing variables to be substituted.
 * @param values_to_substitute An array of strings containing the values to substitute for removed variables.
 * @return A pointer to a newly allocated string containing the new line with substituted variable values.
 *         Memory is allocated dynamically for the new line.
 *         Returns NULL if memory allocation fails or if the arguments are invalid.
 */
char *create_new_line(RemovedVariables *removed_variables, const char *line, char** values_to_substitute) {
    // Calculating Size of New Line
    int new_line_size = strlen(line) - removed_variables->removed;
    int num_removed = removed_variables->num_removed;

    for (int i = 0; i < num_removed; i++) {
        new_line_size += strlen(values_to_substitute[i]);
    }
    new_line_size += 1;
    char *new_line = malloc(sizeof(char) * (new_line_size + 1));
    memset(new_line, '\0', sizeof(char) * (new_line_size + 1));

    char *line_copy = strdup(line);
    char *line_begin = line_copy;
    int i = 0;
    while ((line_copy = strchr(line_begin, '$')) != NULL) {
        strncat(new_line, line_begin, line_copy - line_begin);
        line_begin = line_copy;
        strncat(new_line, values_to_substitute[i], strlen(values_to_substitute[i]));

        line_begin += 1;
        if (*line_begin == '{') {
            line_begin += 2;
        }
        line_begin += strlen(removed_variables->var_names[i]);
        i++;
    }

    strncat(new_line, line_begin, strlen(line_begin));
    new_line[new_line_size] = '\0';

    return new_line;
}
/*
** This function is partially implemented for you, but you may
** scrap the implementation as long as it produces the same result.
**
** Creates a new line on the heap with all named variable *usages*
** replaced with their associated values.
**
** Returns NULL if replacement parsing had an error, or (char *) -1 if
** system calls fail and the shell needs to exit.
*/
char *replace_variables_mk_line(const char *line, Variable *variables){
    RemovedVariables *removedVariables = variable_names(line);
    int num_removed = removedVariables->num_removed;
    char **values_to_substitute = values_from_removed_variables(removedVariables, variables);
    char *newline = create_new_line(removedVariables, line, values_to_substitute);

    for (int i = 0; i < num_removed; i++) {
        free(removedVariables->var_names[i]);
    }
    free(removedVariables->var_names);
    free(removedVariables);

    for (int i = 0; i < num_removed; i++) {
        free(values_to_substitute[i]);
    }
    free(values_to_substitute);

    return newline;
}


void free_variable(Variable *var, uint8_t recursive){
    // TODO:
}