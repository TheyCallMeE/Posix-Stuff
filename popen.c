#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

// Opens a pipe to a forked process that allows it to read or write to the process
FILE* Popen(const char* comm, const char* mode) {
  int fd[2];
  char* args[4];
  int status;
  FILE *fp;

  if (pipe(fd) < 0) {
    return NULL;
  }
  
  args[0] = "/bin/bash";
  args[1] = "-c";
  args[2] = (char *)comm;
  args[3] = NULL;

  // Obtains a file pointer from the file descriptor of the pipe
  if (strcmp(mode, "r") == 0) {
    fp = fdopen(fd[0], "r");
  } else if (strcmp(mode, "w") == 0) {
    fp = fdopen(fd[1], "w");
  }

  pid_t pid = fork();
  if (pid == 0) {  
    if (strcmp(mode, "r") == 0) {
      dup2(fd[1], STDOUT_FILENO);
      close(STDIN_FILENO);
    }
    else if (strcmp(mode, "w") == 0) {
      dup2(fd[0], STDIN_FILENO);
      close(STDOUT_FILENO);
    }
    
    execv(args[0], args);
    abort();
  }
  int *pidptr = (int*)(&fp->_unused2);
  *pidptr = pid;
  if (strcmp(mode, "r") == 0) {
    close(fd[1]);
  }
  else if (strcmp(mode, "w") == 0) {
    close(fd[0]);
  }
  
  return fp;
}

void Pclose(FILE* fp) {
  int pid = *(int *)fp->_unused2;
  fclose(fp);
  kill(pid, SIGTERM);
}

int main(int argc, char* argv[]) {
  if (argc < 3) {
    printf("Please enter a command in quotes and a mode\n");
    return 1;
  }

  FILE *fp = Popen(argv[1], argv[2]);

  if (fp == NULL) {
    printf("Error with popen.\n");
    return 1;
  }
 
  if (strcmp(argv[2], "r") == 0) { 
    char buffer[200];
    while(!feof(fp)) {
      ssize_t n = fread(buffer, 1, 1, fp);
      if (n == 0) {
        break;
      }
      putchar(buffer[0]);
    }
  }
  else if (strcmp(argv[2], "w") == 0) {
    fprintf(fp, "%s", "Gibberishly gibberish\n guange");
    sleep(1);
  }

  Pclose(fp);
  return 0;
}
