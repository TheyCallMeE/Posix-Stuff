#include <iostream>
#include <cstring>
#include <vector>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>

enum class HTTPMethod {
  kGet, kPost
};

class HTTPHeader {
  public:
    HTTPHeader(char* buffer, size_t len);
    ~HTTPHeader() = default;

    const std::string& GetArgs() const { return args_; }
    const std::map<std::string, std::string>& GetMime() const { return mime_; }
    HTTPMethod GetMethod() const { return method_; }
    const std::string& GetURI() const { return uri_; }
    int FindInt(const std::string& to_find);
    const std::string& FindInMime(const std::string& to_find);

  private:
    HTTPMethod method_;
    std::string uri_;
    std::string args_;
    std::map<std::string, std::string> mime_;
};

HTTPHeader::HTTPHeader(char* buffer, size_t len) {
  char method[5];
  char uri[200];
  sscanf(buffer, "%s %s", method, uri);
  if (strcmp(method, "GET") == 0) {
    method_ = HTTPMethod::kGet;
  } else if (strcmp(method, "POST") == 0) {
    method_ = HTTPMethod::kPost;
  }
  uri_ = uri;

  if (method_ == HTTPMethod::kGet) {
    char pathname[4096];
    getcwd(pathname, sizeof(pathname));
    strcat(pathname, uri);
    args_ = pathname;
  } else if (method_ == HTTPMethod::kPost) {
    std::string comm = (uri + 1);
    args_ = comm + " ";
    // Parse the mime header into a std::map
    // Avoid the first line that has already been processed
    char* n_buff = buffer + len;
    while (buffer != n_buff && *buffer != '\r') {
      buffer++;
    }
    buffer += 2;                             // Skips the accompanying \n
    bool processed {false};
    std::string line {};
    while(!processed) {
      if (buffer != n_buff && *buffer == '\r') {
        if (buffer != n_buff && *(buffer + 2) == '\r') {
          processed = true;
          buffer += 2;
        }
        size_t n = line.find(": ");
        if (n == std::string::npos) {
          continue;
        }
        std::string label = line.substr(0, n);
        std::string val = line.substr(n + 2, std::string::npos);
        mime_.insert(std::pair<std::string, std::string>(label, val));
        line.clear();
        buffer += 2;
      } else {
        if (line.empty()) {
          line = *buffer;
        }
        else {
          line += *buffer;
        }
        buffer++;
      }
    }

    // Gather the arguments
    int num_bytes = FindInt("Content-Length");
    if (num_bytes >= 0) {
      for (int i = 0; i < num_bytes; i++) {
        args_ += *buffer;
        buffer++;
      }
    }
  }
}

int HTTPHeader::FindInt(const std::string& to_find) {
  std::map<std::string, std::string>::iterator it;
  it = mime_.find(to_find);
  if (it == mime_.end()) {
    return -1;
  }
  int n = atoi(it->second.c_str());
  return n;
}

const std::string& HTTPHeader::FindInMime(const std::string& to_find) {
  static std::string empty;
  std::map<std::string, std::string>::iterator it;
  it = mime_.find(to_find);
  if (it == mime_.end()) {
    return empty;
  }
  return it->second;
}


class HTTPServer {
  public:
    HTTPServer(int port);
    ~HTTPServer();

    void Run();

  private:
    void SendContents(int sockfd, const char* filename);
    void DoTask(std::string arg, int fd);

    int sockfd_;
    int port_;
    struct sockaddr_in serv_addr_;
};

HTTPServer::HTTPServer(int port) : port_(port) { 
  // Create a socket, port 8080
  sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd_ < 0) {
    std::cerr << "Error opening socket" << std::endl;
  }
  std::cout << "Socket opened" << std::endl;

  // Prepare the sockaddr_in structure
  std::memset(&serv_addr_, 0, sizeof(struct sockaddr_in));
  serv_addr_.sin_family = AF_INET;
  serv_addr_.sin_addr.s_addr = INADDR_ANY;
  serv_addr_.sin_port = htons(port_);

  // Allows successive reconnections to socket without timeout
  int optval = 1;
  setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  // Bind the socket to the current IP address on the port
  if (bind(sockfd_, (struct sockaddr *) &serv_addr_, sizeof(serv_addr_)) < 0) {
    std::cerr << "Error on binding." << std::endl;
  }
  std::cout << "Socket bound" << std::endl;

  // Listen for incoming connections
  listen(sockfd_, 5);
  std::cout << "Socket listening..." << std::endl;

}

HTTPServer::~HTTPServer() {
  close(sockfd_);
}

void HTTPServer::SendContents(int sockfd, const char* filename) {
  char message[200];
  FILE* fp;
  struct stat filestat;
  int status;
  if ((status = stat(filename, &filestat)) != 0) {
    snprintf(message, sizeof(message), "HTTP/1.1 404 Not Found\r\n\r\nFile cannot be opened.\r\n");
    write(sockfd, message, strlen(message) + 1);
    return;
  }

  if ((fp = fopen(filename, "r")) == nullptr) {
    snprintf(message, sizeof(message), "HTTP/1.1 404 Not Found\r\n\r\nFile cannot be opened.\r\n");
    write(sockfd, message, strlen(message) + 1);
  } else {
    snprintf(message, sizeof(message), "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n",
              filestat.st_size);
    write(sockfd, message, strlen(message) + 1);
    char buffer[200];
    while(!feof(fp)) {
      ssize_t n = fread(buffer, 1, sizeof(buffer), fp);
      snprintf(message, n + 1, "%s", buffer);
      ssize_t a = write(sockfd, message, n);
      ssize_t diff = n - a;
      while (diff > 0) {
        a = write(sockfd, message + a, diff);
        diff -= a;
      }
    }
    fclose(fp);
  }
}

void HTTPServer::DoTask(std::string args, int fd) {
  char* comm[4];

  char temp[256];
  snprintf(temp, sizeof(temp), "/tmp/fooXXXXXX");
  args.append("> " + std::string(temp));

  std::string path {"/bin/bash"};
  std::string c {"-c"};

  comm[0] = (char *) path.c_str();
  comm[1] = (char *) c.c_str();
  comm[2] = (char *) args.c_str();
  comm[3] = NULL;

  pid_t pid = fork();
  if (pid == 0) {
    execv(comm[0], comm);
    exit(1);
  }
  int status;
  waitpid(pid, &status, 0);
  if (WIFEXITED(status)) {
    int exit_status = WEXITSTATUS(status);
    if (exit_status == 0) {
      SendContents(fd, temp);
      return;
    }
  }
  SendContents(fd, "/filedoesntexist");
}

void HTTPServer::Run() {
  int client_sock;
  struct sockaddr_in cli_addr;
  char buffer[256];

  while (true) { 
    // Accept any incoming connections, this is blocking
    socklen_t clilen = sizeof(cli_addr);
    client_sock = accept(sockfd_, (struct sockaddr *) &cli_addr, &clilen);
    if (client_sock < 0) {
      std::cerr << "Error on accepting." << std::endl;
    }

    ssize_t n = read(client_sock, buffer, 255);
    if (n < 0) {
      std::cerr << "Error reading from socket." << std::endl;
    }

    HTTPHeader header(buffer, n);

    if (header.GetMethod() == HTTPMethod::kGet) {
      SendContents(client_sock, header.GetArgs().c_str());
    } else if (header.GetMethod() == HTTPMethod::kPost) {
      DoTask(header.GetArgs(), client_sock); 
    }

    close(client_sock);
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cout << "Error: no port provided" << std::endl;
    return 1;
  }

  HTTPServer server(atoi(argv[1]));

  server.Run();

  return 0;
}
