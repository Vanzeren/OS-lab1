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

#include "parse.h"

static void print_cmd(Command *cmd);
static void print_pgm(Pgm *p);
void handle_cmd(Command *cnd);
void sigint_handler(int sig);

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
        handle_cmd(&cmd);
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

void handle_cmd(Command *cmd)
{
  if (cmd->pgm == NULL)
  {
    return;
  }

  // For single command (no pipes)
  if (cmd->pgm->next == NULL)
  {
    pid_t pid = fork();

    if (pid == 0)
    {

      // Child process
      if (cmd->background)
      {
        pid_t sid = setsid();
        if (sid < 0)
        {
          perror("setsid");
          exit(EXIT_FAILURE);
        }
      }

      // Handle input redirection
      if (cmd->rstdin != NULL)
      {
        FILE *input_file = freopen(cmd->rstdin, "r", stdin);
        if (input_file == NULL)
        {
          perror("freopen stdin");
          exit(EXIT_FAILURE);
        }
      }

      // Handle output redirection
      if (cmd->rstdout != NULL)
      {
        FILE *output_file = freopen(cmd->rstdout, "w", stdout);
        if (output_file == NULL)
        {
          perror("freopen stdout");
          exit(EXIT_FAILURE);
        }
      }

      // Handle error redirection
      if (cmd->rstderr != NULL)
      {
        FILE *error_file = freopen(cmd->rstderr, "w", stderr);
        if (error_file == NULL)
        {
          perror("freopen stderr");
          exit(EXIT_FAILURE);
        }
      }

      if (cmd->background)
      {
        signal(SIGHUP, SIG_IGN);
      }

      // Execute the command
      execvp(cmd->pgm->pgmlist[0], cmd->pgm->pgmlist);

      // If execvp returns, there was an error
      perror("execvp");
      exit(EXIT_FAILURE);
    }
    else if (pid > 0)
    {
      // Parent process
      if (cmd->background)
      {
        // Print background process PID
        printf("[%d]\n", pid);
        fflush(stdout);
      }
      else
      {
        // Wait for foreground process to complete
        int status;
        waitpid(pid, &status, 0);
      }
    }
    else
    {
      // Fork failed
      perror("fork");
    }
  }
  else
  {
    // TODO: Handle piped commands
    printf("Piped commands not yet implemented\n");
  }
}
