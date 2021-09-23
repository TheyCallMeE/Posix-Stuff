#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define BUF_SIZE 1024

// Directory entry structure
struct ps_dirent {
  uint64_t d_ino;
  off_t d_off;
  uint16_t d_reclen;
  char d_name[];
};

// Conversion of string to an int64_t
int64_t stringToDecimal(char *num) {
  int64_t res = 0;

  while (isdigit(*num)) {
    res *= 10;
    res += (*num - '0');
    num++;
  }

  return res;
}

// Obtain the total memory of the system using /proc/meminfo
int64_t totalMem() {
  FILE *fp = fopen("/proc/meminfo", "r");
  if (fp == NULL) {
    perror("Reading meminfo");
    abort();
  }

  char *line = NULL;
  size_t len = 0;
  // The total memory is the first line in /proc/meminfo
  ssize_t nread = getline(&line, &len, fp);
  if (nread == -1) {
    perror("Reading meminfo.");
    abort();
  }

  // Extracting the number
  char mem[16];
  for (int i = 0; i < 16;) {
    if (isdigit(*line)) {
      mem[i] = *line;
      i++;
    } else if (*(line + 1) == 'k') {  // Know the format is # kb, break on the k
      mem[i] = '\0';
      break;
    }
    line++;
  }

  fclose(fp);

  return stringToDecimal(mem);
}

// Uses the rss and divides by the total memory to give a percent usage
double calculateMemPercent(int64_t rss, int64_t mem) {
  double percent = (double)rss / (double)mem;
  return percent * 100;
}

// Obtains the RSS from /proc/pid/stat in kb
int64_t getRSS(char *buff) {
  char buffer[strlen(buff)];
  strcpy(buffer, buff);
  // Skips the pid and (comm)
  char *paren = strchr(buffer, ')');
  paren++;
  if (*paren == ')') paren++;  // Comm might be in the format of ((comm))
  char *tok = strtok(paren, " ");
  for (int i = 0; i < 21;
       i++) {  // From man/proc rss is the 24th entry in /stat
    tok = strtok(NULL, " ");
  }
  int64_t pages = stringToDecimal(tok);
  pages *= 4096;  // 4096 bits per rss page
  pages /= 1024;  // 1024 bits in a kilobit
  return pages;
}

// Obtains the VSZ from /proc/pid/stat in kb
int64_t getVSZ(char *buff) {
  char buffer[strlen(buff)];
  strcpy(buffer, buff);
  // Skips the pid and (comm)
  char *paren = strchr(buffer, ')');
  paren++;
  if (*paren == ')') paren++;  // Comm might be in the format of ((comm))
  char *tok = strtok(paren, " ");
  for (int i = 0; i < 20;
       i++) {  // From man/proc vsz is the 23rd entry in /stat
    tok = strtok(NULL, " ");
  }

  int64_t vm = stringToDecimal(tok);
  vm /= 1024;  // 1024 bits in a kilobit
  return vm;
}

// Obtain the owner of the process from /proc/pid
void getOwner(char *pid, char *owner) {
  char path[16] = "/proc/";
  strcat(path, pid);

  // Read the /proc/pid directory into a stat structure
  struct stat dirstat;
  int status;
  if ((status = stat(path, &dirstat)) != 0) {
    perror("Error opening directory.");
    abort();
  }

  // The UID of the directory, same as of the process
  uid_t uid = dirstat.st_uid;

  // Mapping the UID to the user within the passwd file
  struct passwd *pwd = getpwuid(uid);
  if (pwd == NULL) {
    perror("Error finding user using given uid.");
    abort();
  }

  strcpy(owner, pwd->pw_name);
}

// Obtain the process's command from /proc/pid/comm
void getComm(char *pid, char *comm) {
  char path[22];
  snprintf(path, sizeof(path), "/proc/%s/comm", pid);

  FILE *fp = fopen(path, "r");

  char buffer[200];
  long int buffsize = 200;
  while (!feof(fp)) {
    size_t n = fread(buffer, 1, buffsize, fp);
    if (buffer[n - 1] ==
        '\n') {  // /comm ends with a newline character, remove it
      buffer[n - 1] = '\0';
    } else {
      buffer[n] = '\0';
    }
  }

  fclose(fp);

  strcpy(comm, buffer);
}

// Obtain the current state of the process from /proc/pid/stat
void getState(char *buff, char *state) {
  char buffer[strlen(buff)];
  strcpy(buffer, buff);
  // Skips the pid and (comm)
  char *paren = strchr(buffer, ')');
  paren++;
  if (*paren == ')') paren++;      // comm can be in the format of ((comm))
  char *tok = strtok(paren, " ");  // The state is the first entry after (comm)
  strcpy(state, tok);
}

// Obtains the time of conception of the process using the boot time of the
// computer
void getBirth(char *buff, char *birth) {
  // First get the boot time from /proc/stat
  FILE *fp = fopen("/proc/stat", "r");
  if (fp == NULL) {
    perror("Reading meminfo");
    abort();
  }

  char *line = NULL;
  size_t len = 0;
  for (int i = 0; i < 8; i++) {  // Know it is the 9th entry of the /stat file
    ssize_t nread = getline(&line, &len, fp);
    if (nread == -1) {
      perror("Reading meminfo.");
      abort();
    }
  }

  // Extract the time
  char btime[11];
  for (int i = 0; i < 11;) {
    if (isdigit(*line)) {
      btime[i] = *line;
      i++;
    } else if (*(line) == '\n') {
      btime[i] = '\0';
      break;
    }
    line++;
  }

  fclose(fp);

  int64_t boot = stringToDecimal(btime);

  // Obtain the startup time of the process
  char buffer[strlen(buff)];
  strcpy(buffer, buff);
  // Skip the pid and (comm)
  char *paren = strchr(buffer, ')');
  paren++;
  if (*paren == ')') paren++;  // comm can be in the form ((comm))
  char *tok = strtok(paren, " ");
  for (int i = 0; i < 19;
       i++) {  // From man proc the start time is the 22nd entry in /stat
    tok = strtok(NULL, " ");
  }
  int64_t ticks = stringToDecimal(tok);  // starttime in /stat is in clock ticks
  int64_t time = ticks / sysconf(_SC_CLK_TCK);  // Convert to seconds

  time += boot;

  // Use the boot time and starttime together to find the conception of the
  // program
  time_t t = time;
  char *calendar = ctime(&t);

  // Output is DAY MONTH DATE HH:MM:SS YEAR
  strcpy(birth, calendar);
}

// Obtain the tty of the process from /proc/pid/fd/0
void getTTY(char *pid, char *tty) {
  char path[22];
  char temp[14];
  snprintf(path, sizeof(path), "/proc/%s/fd/0", pid);
  // /proc/pid/fd/0 is a symbolic link
  ssize_t n = readlink(path, temp, sizeof(temp));
  // Unable to read (due to permissions or otherwise)
  if (n < 0) {
    strcpy(tty, "?");
    return;
  }
  temp[n] = '\0';
  strcpy(tty, temp);
}

int main(int argc, char *argv[]) {
  double percent;
  int64_t rss;
  int64_t vsz;
  char owner[50];
  char state[2];
  char comm[100];
  char time[25];
  char tty[14];
  char buffer[200];
  char path[22];
  FILE *fp;

  printf("%-16s %-8s %-7s %-5s %-6s %-8s %-14s %-33s %s\n", "Owner", "PID",
         "State", "Mem%", "RSS", "VSZ", "tty", "Comm", "Started");

  char buf[BUF_SIZE];  // For parsing the directory
  struct ps_dirent *d;

  int64_t mem = totalMem();

  int fd = open("/proc", O_RDONLY | O_DIRECTORY);
  if (fd == -1) {
    perror("error opening directory");
    return 1;
  }

  for (;;) {
    int64_t nread = syscall(SYS_getdents, fd, buf, BUF_SIZE);
    if (nread == -1) {
      perror("Error reading contents.");
      return 1;
    }
    if (nread == 0) break;

    for (int64_t bpos = 0; bpos < nread;) {
      d = (struct ps_dirent *)(buf + bpos);
      if (isdigit(d->d_name[0])) {
        snprintf(path, sizeof(path), "/proc/%s/stat", d->d_name);

        fp = fopen(path, "r");
        if (fp == NULL) {
          perror("open");
          abort();
        }

        ssize_t n = fread(buffer, 1, sizeof(buffer), fp);
        if (n < 0) {
          perror("fread");
          abort();
        }

        fclose(fp);

        rss = getRSS(buffer);
        vsz = getVSZ(buffer);
        getComm(d->d_name, comm);
        getOwner(d->d_name, owner);
        getState(buffer, state);
        percent = calculateMemPercent(rss, mem);
        getBirth(buffer, time);
        getTTY(d->d_name, tty);

        printf("%-16s %-8s %-7s %-5.1f %-6ld %-8ld %-14s %-33s %s\n", owner,
               d->d_name, state, percent, rss, vsz, tty, comm, time);
      }
      bpos += d->d_reclen;
    }
  }

  return 0;
}
