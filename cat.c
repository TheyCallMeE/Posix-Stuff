#include <stdio.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
  if (argc < 2) {
    printf("Usage: supply a file to read\n");
    return 1;
  }

  char buffer[200];
  FILE *fptr;
  if ((fptr = fopen(argv[1], "r")) == NULL) {
    perror("fopen");
    return 1;
  }
  while (!feof(fptr)) {
    size_t n = fread(buffer, 1, sizeof(buffer), fptr);
    if (n != (sizeof(buffer) - 1)) {
      buffer[n] = '\0';
    }
    printf("%s", buffer);
  }
  fclose(fptr);

  return 0;
}
