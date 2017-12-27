//Omar Iltaf

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

//Constant values
#define MAX_ARGS 512
#define MAX_LINE_LENGTH 2048
#define DELIMITERS " \t\r\n\a"

//Global variables
int exitStatus;
int bgCounter = 0;
pid_t smallshPid;
int foregroundOnly = 0;

//Main struct for the structure of a line
struct LineContents {
  int isComment;
  int isBackground;

  char* command;
  char** args;

  int redirStdIn;
  int redirStdOut;

  char* inputFile;
  char* outputFile;
};

//Function to replace a certain substring within a string with a given string
char* ReplaceWord(const char* s, const char* oldW, const char* newW) {
  char* result;
  int i, count = 0;
  int newWlen = strlen(newW);
  int oldWlen = strlen(oldW);
  //Counts the number of times oldW occurs in s
  for (i = 0; s[i] != '\0'; i++) {
    if (strstr(&s[i], oldW) == &s[i]) {
      count++;
      //Jumping to index after the old word
      i += oldWlen - 1;
    }
  }

  //Making new string of appropriate length
  result = (char*) malloc(i + count * (newWlen - oldWlen) + 1);
  i = 0;
  while (*s) {
    //Compares the substring with the result
    if (strstr(s, oldW) == s) {
      strcpy(&result[i], newW);
      i += newWlen;
      s += oldWlen;
    } else {
      result[i++] = *s++;
    }
  }
  result[i] = '\0';
  return result;
}

//Uses passed in tokenized strings to generate line struct
struct LineContents* CreateLineContentsStruct(char** tokens, int length) {
  struct LineContents* lineContents = malloc(sizeof(struct LineContents));
  lineContents->isComment = 0;
  lineContents->isBackground = 0;
  lineContents->command = NULL;
  lineContents->args = malloc(MAX_ARGS * sizeof(char*));
  lineContents->redirStdIn = 0;
  lineContents->redirStdOut = 0;
  lineContents->inputFile = NULL;
  lineContents->outputFile = NULL;

  if (strcmp(tokens[length - 1], "&") == 0) {
    lineContents->isBackground = 1;
  }

  if (strcmp(tokens[0], "#") == 0) {
    lineContents->isComment = 1;
  } else if (tokens[0][0] == '#') {
    lineContents->isComment = 1;
  }

  int argsIndex = 0;
  if (lineContents->isComment != 1) {
    lineContents->command = tokens[0];
    lineContents->args[argsIndex] = tokens[0];
    argsIndex++;
    for (int i = 1; i < length; i++) {
      if (strcmp(tokens[i], "<") == 0) {
        lineContents->redirStdIn = 1;
        i++;
        lineContents->inputFile = tokens[i];
      } else if (strcmp(tokens[i], ">") == 0) {
        lineContents->redirStdOut = 1;
        i++;
        lineContents->outputFile = tokens[i];
      } else {
        if (strcmp(tokens[i], "&") != 0) {
          //This part expands "$$" to obtain the process id
          if (strstr(tokens[i], "$$") != NULL) {
            char pid[24];
            sprintf(pid, "%d", getpid());
            tokens[i] = ReplaceWord(tokens[i], "$$", pid);
          }
          lineContents->args[argsIndex] = tokens[i];
          argsIndex++;
        }
      }
    }
    lineContents->args[argsIndex] = NULL;
  }

  return lineContents;
}

//Reads in user input
char* ReadLine() {
  char* line = NULL;
  ssize_t bufferSize = 0;
  getline(&line, &bufferSize, stdin);
  return line;
}

//Splits apart a line separating all strings with spaces inbetween
struct LineContents* SplitLine(char* line) {
  int bufferSize = MAX_LINE_LENGTH;
  int position = 0;
  char** tokens = malloc(bufferSize * sizeof(char*));
  char* token;

  if (!tokens) {
    fprintf(stderr, "smallsh: allocation error\n");
    fflush(stdout);
    exit(1);
  }

  token = strtok(line, DELIMITERS);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    token = strtok(NULL, DELIMITERS);
  }

  tokens[position] = NULL;
  return CreateLineContentsStruct(tokens, position);
}

//Helper function to print out contents of a line struct
void PrintLineContentsStruct(struct LineContents* lineContents) {
  if (lineContents->command != NULL) {
    printf("Command = %s\n", lineContents->command);
    fflush(stdout);
  } else {
    printf("Command = NONE\n");
    fflush(stdout);
  }
  int index = 0;
  while (lineContents->args[index] != NULL) {
    printf("args = %s\n", lineContents->args[index]);
    fflush(stdout);
    index++;
  }
  if (lineContents->inputFile != NULL) {
    printf("InputFile = %s\n", lineContents->inputFile);
    fflush(stdout);
  } else {
    printf("InputFile = NONE\n");
    fflush(stdout);
  }
  if (lineContents->outputFile != NULL) {
    printf("OutputFile = %s\n", lineContents->outputFile);
    fflush(stdout);
  } else {
    printf("OutputFile = NONE\n");
    fflush(stdout);
  }
  if (lineContents->isBackground) {
    printf("InBackground = YES\n");
    fflush(stdout);
  } else {
    printf("InBackground = NO\n");
    fflush(stdout);
  }
}

///////////////////////////////////////////////////////////////////////////////

//Handles the running of non-built-in commands in both the fore- and backgrounds
int Launch(struct LineContents* lineContents) {
  pid_t spawnPid = -5;
  pid_t wPid = -5;
  int childExitStatus = -5;
  //Creates a new child process
  spawnPid = fork();
  switch (spawnPid) {
    case -1: {
      //Error in forking
      perror("smallsh");
      fflush(stdout);
      exit(1);
      break;
    }
    case 0: {
      //Child Process
      int sourceFD, targetFD, result;
      //Checks if user has redirected stdin
      if (lineContents->redirStdIn) {
        sourceFD = open(lineContents->inputFile, O_RDONLY);
        if (sourceFD == -1) {
          printf("cannot open %s for input\n", lineContents->inputFile);
          fflush(stdout);
          exit(1);
        }
        //Redirecting stdin
        result = dup2(sourceFD, 0);
        if (result == -1) {
          exit(2);
        }
      } else if (lineContents->isBackground && !foregroundOnly) {
        //Sets default stdin for background process
        sourceFD = open("/dev/null", O_RDONLY);
        if (sourceFD == -1) {
          printf("cannot open %s for input\n", "/dev/null");
          fflush(stdout);
          exit(1);
        }
        result = dup2(sourceFD, 0);
        if (result == -1) {
          exit(2);
        }
      }
      //Checks if user has redirected stdout
      if (lineContents->redirStdOut) {
        targetFD = open(lineContents->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (targetFD == -1) {
          printf("cannot open %s for output\n", lineContents->outputFile);
          fflush(stdout);
          exit(1);
        }
        //Redirecting stdout
        result = dup2(targetFD, 1);
        if (result == -1) {
          exit(2);
        }
      } else if (lineContents->isBackground && !foregroundOnly) {
        //Sets default stdout for background process
        targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (targetFD == -1) {
          printf("cannot open %s for output\n", "/dev/null");
          fflush(stdout);
          exit(1);
        }
        result = dup2(targetFD, 1);
        if (result == -1) {
          exit(2);
        }
      }
      if (lineContents->isBackground && !foregroundOnly) {
        setpgid(spawnPid, 0);
      }
      //Actual execution of command, error message if command is invalid
      if (execvp(lineContents->args[0], lineContents->args) == -1) {
        perror(lineContents->command);
        fflush(stdout);
      }
      exit(1);
      break;
    }
    default: {
      //Parent Process
      //Checks if the child process was run in foreground or background
      if (!lineContents->isBackground || foregroundOnly) {
        wPid = waitpid(spawnPid, &childExitStatus, 0);
        if (WIFEXITED(childExitStatus)) {
          exitStatus = WEXITSTATUS(childExitStatus);
        } else if (WIFSIGNALED(childExitStatus)) {
          exitStatus = WTERMSIG(childExitStatus);
        }
      } else {
        //If run in background, a counter of background processes is incremented
        bgCounter++;
        printf("background pid is %d\n", spawnPid);
        fflush(stdout);
      }
      return 1;
      break;
    }
  }

  return 1;
}

//Function to check if any background processes have completed and are in a zombie state
void CheckBackground() {
  pid_t wPid = -6;
  int childExitStatus = -6;
  int bgExitStatus;

  // do {
    wPid = waitpid(-1, &childExitStatus, WNOHANG);
    if (wPid > 0) {
      if (WIFEXITED(childExitStatus)) {
        bgExitStatus = WEXITSTATUS(childExitStatus);
        printf("background pid %d is done: exit value %d\n", wPid, bgExitStatus);
        fflush(stdout);
      } else if (WIFSIGNALED(childExitStatus)) {
        bgExitStatus = WTERMSIG(childExitStatus);
        printf("background pid %d is done: terminated by signal %d\n", wPid, bgExitStatus);
        fflush(stdout);
      }
      bgCounter--;
      // printf("%d\n", bgCounter);
      CheckBackground();
    }
  // } while (!WIFEXITED(childExitStatus) && !WIFSIGNALED(childExitStatus));
}

//Contains all supported built-in commands
char* BuiltInStr[] = {
  "cd",
  "exit",
  "status"
};

//Number of built-in commands
int NumBuiltIns() {
  return sizeof(BuiltInStr) / sizeof(char*);
}

//Built-in "cd" command
int CommandCD(struct LineContents* lineContents) {
  if (lineContents->args[1] == NULL) {
    char* home = getenv("HOME");
    if (chdir(home) != 0) {
      perror("smallsh");
      fflush(stdout);
    }
  } else {
    if (chdir(lineContents->args[1]) != 0) {
      perror("smallsh");
      fflush(stdout);
    }
  }
  exitStatus = 0;
  return 1;
}

//Built-in "exit" command
int CommandExit() {
  exitStatus = 0;
  return 0;
}

//Built-in "status" command
int CommandStatus(struct LineContents* lineContents) {
  printf("exit value %d\n", exitStatus);
  fflush(stdout);
  exitStatus = 0;
  return 1;
}

//Main function to determine whether a command is built-in or not
int Execute(struct LineContents* lineContents) {
  if (lineContents->args[0] == NULL) {
    return 1;
  }
  if (strcmp(lineContents->args[0], BuiltInStr[0]) == 0) {
    return CommandCD(lineContents);
  }
  if (strcmp(lineContents->args[0], BuiltInStr[1]) == 0) {
    return CommandExit();
  }
  if (strcmp(lineContents->args[0], BuiltInStr[2]) == 0) {
    return CommandStatus(lineContents);
  }
  return Launch(lineContents);
}

///////////////////////////////////////////////////////////////////////////////

//Signal handler for SIGINT
void catchSIGINT(int signo) {
  char* message = "terminated by signal 2\n: ";
  write(STDOUT_FILENO, message, 25);
  fflush(stdout);
  // return;
}

//Signal handler for SIGTSTP - toggles in and out of foreground-only mode
void catchSIGTSTP(int signo) {
  if (foregroundOnly == 0) {
    foregroundOnly = 1;
    char* message = "\nEntering foreground-only mode (& is now ignored)\n: ";
    write(STDOUT_FILENO, message, 52);
    fflush(stdout);
  } else {
    foregroundOnly = 0;
    char* message = "\nExiting foreground-only mode\n: ";
    write(STDOUT_FILENO, message, 32);
    fflush(stdout);
  }
  // return;
}

///////////////////////////////////////////////////////////////////////////////

//Main part of program
int main(int argc, char const *argv[]) {
  //Variables
  char* line;
  char** args;
  struct LineContents* lineContents;
  int status = 1;

  //Registers signal handlers
  signal(SIGINT, catchSIGINT);
  signal(SIGTSTP, catchSIGTSTP);

  //Main loop of program
  do {
    CheckBackground();
    printf(": ");
    fflush(stdout);
    line = ReadLine();
    //If line not empty
    if (strlen(line) != 1) {
      lineContents = SplitLine(line);
      //If line not a comment
      if (!lineContents->isComment) {
        // PrintLineContentsStruct(lineContents);
        status = Execute(lineContents);
        free(line);
        free(args);
      }
      free(lineContents->args);
      free(lineContents);
    }
  } while (status);
  return 0;
}
