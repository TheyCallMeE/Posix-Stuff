#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

int main(int argc, char* argv[]) {
  if (argc < 3) {
    printf("Usage: supply a source file and destination file\n");
    return 1;
  }

  int srcfd = open(argv[1], O_RDONLY);
  if (srcfd < 0) {
    perror("open");
    return 1;
  }

  int destfd = open(argv[2], O_WRONLY | O_CREAT);
  if (destfd < 0) {
    perror("open");
    return 1;
  }

  // Necessary to copy the permissions of the source file to the destination
  struct stat st;
  stat(argv[1], &st);
  chmod(argv[2], st.st_mode);

  char buffer[200];
  ssize_t n;
  while ((n = read(srcfd, buffer, sizeof(buffer))) != 0) {
    if (n < 0) {
      perror("read");
      return 1;
    }
    if (n < sizeof(buffer)) {
      buffer[n] = '\0';
    }

    ssize_t a = write(destfd, buffer, n);
    if (a < 0) {
      perror("write");
      return 1;
    }

    ssize_t diff = n - a;
    while (diff > 0) {
      a = write(destfd, buffer + a, diff);
      diff -= a;
    }
  }

  close(srcfd);
  close(destfd);
}
