/*
 * Main source code file for lsh shell program
 *
 * You are free to add functions to this file.
 * If you want to add functions in a separate file(s)
 * you will need to modify the CMakeLists.txt to compile
 * your additional file(s).
 *
 * Add appropriate comments in your code to make it
 * easier for us while grading your assignment.
 *
 * Using assert statements in your code is a great way to catch errors early and make debugging easier.
 * Think of them as mini self-checks that ensure your program behaves as expected.
 * By setting up these guardrails, you're creating a more robust and maintainable solution.
 * So go ahead, sprinkle some asserts in your code; they're your friends in disguise!
 *
 * All the best!
 */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>

// The <unistd.h> header is your gateway to the OS's process management facilities.
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "parse.h"

static void print_cmd(Command *cmd);
static void print_pgm(Pgm *p);
void handle_cmd(Command *cnd);
void sigint_handler(int sig);
void stripwhite(char *string);
void execute_pipeline(Command *cmd);

int main(void)
{
  for (;;)
  {
    char *line;
    line = readline("> ");
    // handle EOF(ctrl-d)
    if (line == NULL)
    {
      printf("EOF\n");
      exit(EXIT_SUCCESS);
    }
    // Remove leading and trailing whitespace from the line
    stripwhite(line);

    // If stripped line not blank
    if (*line)
    {
      add_history(line);

      Command cmd;
      if (parse(line, &cmd) == 1)
      {
        // Execute the command
        execute_pipeline(&cmd);
        // print_cmd(&cmd);
      }
      else
      {
        printf("Parse ERROR\n");
      }
    }

    // Clear memory
    free(line);
  }

  return 0;
}

/*
 * Print a Command structure as returned by parse on stdout.
 *
 * Helper function, no need to change. Might be useful to study as inspiration.
 */
static void print_cmd(Command *cmd_list)
{
  printf("------------------------------\n");
  printf("Parse OK\n");
  printf("stdin:      %s\n", cmd_list->rstdin ? cmd_list->rstdin : "<none>");
  printf("stdout:     %s\n", cmd_list->rstdout ? cmd_list->rstdout : "<none>");
  printf("background: %s\n", cmd_list->background ? "true" : "false");
  printf("Pgms:\n");
  print_pgm(cmd_list->pgm);
  printf("------------------------------\n");
}

/* Print a (linked) list of Pgm:s.
 *
 * Helper function, no need to change. Might be useful to study as inpsiration.
 */
static void print_pgm(Pgm *p)
{
  if (p == NULL)
  {
    return;
  }
  else
  {
    char **pl = p->pgmlist;

    /* The list is in reversed order so print
     * it reversed to get right
     */
    print_pgm(p->next);
    printf("            * [ ");
    while (*pl)
    {
      printf("%s ", *pl++);
    }
    printf("]\n");
  }
}

/* Strip whitespace from the start and end of a string.
 *
 * Helper function, no need to change.
 */
void stripwhite(char *string)
{
  size_t i = 0;

  while (isspace(string[i]))
  {
    i++;
  }

  if (i)
  {
    memmove(string, string + i, strlen(string + i) + 1);
  }

  i = strlen(string) - 1;
  while (i > 0 && isspace(string[i]))
  {
    i--;
  }

  string[++i] = '\0';
}

/**
 * Executes a pipeline of commands.
 * This is a robust function for handling pipelines.
 */
void execute_pipeline(Command *cmd) {
    if (cmd->pgm == NULL) {
        return;
    }

    //  reverse pgm 
    Pgm *prev = NULL;
    Pgm *current = cmd->pgm;
    Pgm *next = NULL;
    
    while (current != NULL) {
        next = current->next;
        current->next = prev;
        prev = current;
        current = next;
    }
    cmd->pgm = prev;
    
    Pgm *current_pgm = cmd->pgm;

    int input_fd = STDIN_FILENO;
    int pid_count = 0;
    pid_t pids[100];

    while (current_pgm != NULL) {
        int pipefd[2] = {-1, -1};
        
        // Create a new pipe if there are more commands in the chain
        if (current_pgm->next != NULL) {
            if (pipe(pipefd) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Child process
            if (cmd->background) {
                if (setsid() < 0) {
                    perror("setsid");
                    exit(EXIT_FAILURE);
                }
            }

            // Redirect stdin from previous pipe or file
            if (input_fd != STDIN_FILENO) {
                dup2(input_fd, STDIN_FILENO);
                close(input_fd);
            } else if (cmd->rstdin != NULL) {
                int fd = open(cmd->rstdin, O_RDONLY);
                if (fd == -1) {
                    perror("open stdin");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            // Redirect stdout to new pipe or file
            if (current_pgm->next != NULL) {
                close(pipefd[0]); // Close read end of the new pipe
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            } else if (cmd->rstdout != NULL) {
                int fd = open(cmd->rstdout, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd == -1) {
                    perror("open stdout");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            
            // Handle stderr redirection
            if (cmd->rstderr != NULL) {
                int fd = open(cmd->rstderr, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd == -1) {
                    perror("open stderr");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
            
            // Close unused pipe ends in the child
            // We already redirected the necessary ends, so close all pipe fds
            if (pipefd[0] != -1) close(pipefd[0]);
            if (pipefd[1] != -1) close(pipefd[1]);
            if (input_fd != STDIN_FILENO) {
                close(input_fd);
            }
            
            execvp(current_pgm->pgmlist[0], current_pgm->pgmlist);
            perror("execvp");
            exit(EXIT_FAILURE);
        } else {
            // Parent process
            if (input_fd != STDIN_FILENO) {
                close(input_fd);
            }
            if (current_pgm->next != NULL) {
                close(pipefd[1]); // Parent doesn't need the write end of the new pipe
                input_fd = pipefd[0]; // Set the read end for the next iteration
            }
            pids[pid_count++] = pid;
            current_pgm = current_pgm->next;
        }
    }

    // Wait for all child processes to complete
    if (!cmd->background) {
        for (int i = 0; i < pid_count; ++i) {
            int status;
            waitpid(pids[i], &status, 0);
        }
    } else {
        printf("Background process PIDs: ");
        for (int i = 0; i < pid_count; ++i) {
            printf("[%d] ", pids[i]);
        }
        printf("\n");
        fflush(stdout);
    }
}
