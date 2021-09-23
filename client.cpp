#include <iostream>
#include <cstring>
#include <vector>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

enum class HTTPMethod {
  kGet, kPost
};

class HTTPClient {
  public:
    HTTPClient(char* hostname, int port, char* method, char* command);
    ~HTTPClient() = default;

    void SendMessage();
    void ReadContents();
    int FindInt(const std::map<std::string, std::string>& mime, const std::string& to_find);
    const std::string& FindInMime(const std::map<std::string, std::string>& mime, const std::string& to_find);

  private:
    HTTPMethod method_;
    std::string comm_;
    std::string host_;
    int port_;
    int sockfd_;
    struct sockaddr_in serv_addr_;
    struct hostent *server_;
};

HTTPClient::HTTPClient(char* hostname, int port, char* method, char* command) 
                       : comm_(command), host_(hostname), port_(port) {
  // Determine the nature of the client's request
  if (strcmp(method, "GET") == 0) {
    method_ = HTTPMethod::kGet;
  } else if (strcmp(method, "POST") == 0) {
    method_ = HTTPMethod::kPost;
  }

  sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd_ < 0) {
    std::cerr << "Error opening socket" << std::endl;
    exit(1);
  }
  std::cout << "Socket opened." << std::endl;

  server_ = gethostbyname(hostname);
  if (server_ == nullptr) {
    std::cerr << "Error: no such host." << std::endl;
    exit(1);
  }

  std::memset(&serv_addr_, 0, sizeof(struct sockaddr_in));
  serv_addr_.sin_family = AF_INET;
  std::memcpy((char *) &serv_addr_.sin_addr.s_addr, server_->h_addr_list[0], server_->h_length);
  serv_addr_.sin_port = htons(port_);

  if (connect(sockfd_, (struct sockaddr *) &serv_addr_, sizeof(serv_addr_)) < 0) {
    std::cerr << "Error connecting" << std::endl;
    exit(1);
  }
  std::cout << "Connected to server." << std::endl;
}

void HTTPClient::SendMessage() {
  char message[200];
  if (method_ == HTTPMethod::kGet) {
    snprintf(message, sizeof(message), "GET /%s HTTP/1.1\r\nHost: %s:%d\r\nAccept: */*\r\n\r\n", comm_.c_str(), host_.c_str(), port_);
  } else if (method_ == HTTPMethod::kPost) {
    int bytes;
    std::string args;
    std::string uri;
    size_t n = comm_.find(" ");
    // Command with no arguments
    if (n == std::string::npos) {
      uri = comm_;
      args = "";
      bytes = 0;
    } else {
      uri = comm_.substr(0, n);
      args = comm_.substr(n + 1, std::string::npos);
      bytes = args.length();
    }
    snprintf(message, sizeof(message), "POST /%s HTTP/1.1\r\nHost: %s:%d\r\nAccept: */*\r\nContent-Length: %d\r\n\r\n%s", uri.c_str(), host_.c_str(), port_, bytes, args.c_str());
  }
  ssize_t n = write(sockfd_, message, sizeof(message));
  if (n < 0) {
    std::cerr << "Error writing to socket" << std::endl;
    return;
  }
}

void HTTPClient::ReadContents() {
  char reply[200];
  char* p = reply;
  int bytes_to_read {0};
  bool header_processed {false};
  std::map<std::string, std::string> mime;
  for (;;) {
    int offset = 0;
    ssize_t n = read(sockfd_, reply, sizeof(reply));
    char* n_reply = reply + n; 
    std::string line;
    if (n < 0) {
      std::cerr << "read failed" << std::endl;
      abort();
    } else if (n == 0) break;
    while (!header_processed) {
      if (p != n_reply && *p == '\r') {
        if (p != n_reply && *(p + 2) == '\r') {
          size_t x = line.find(": ");
          if (x == std::string::npos) {
            continue;
          }
          std::string label = line.substr(0, x);
          std::string val = line.substr(x + 2, std::string::npos);
          mime.insert(std::pair<std::string, std::string>(label, val));
          line.clear();
          // Skip to the content of the message
          p += 4;
          offset += 4;
          header_processed = true;
        }
        else {
          line.clear();
          p += 2;
          offset += 2;
        }
      } else {
        if (line.empty()) {
          line = *p;
        } else {
          line += *p;
        }
        p++;
        offset++;
      }
      bytes_to_read = FindInt(mime, "Content-Length");
    }

    if (bytes_to_read <= 0) break;        // Check that header was processed correctly

    // Write the content to standard output
    fwrite(reply + offset, 1, n - offset, stdout);
    bytes_to_read -= n - offset;
    if (bytes_to_read <= 0) break;        // Logic check
  }
}

int HTTPClient::FindInt(const std::map<std::string, std::string>& mime, const std::string& to_find) {
  std::map<std::string, std::string>::const_iterator it;
  it = mime.find(to_find);
  if (it == mime.end()) {
    return -1;
  }
  int n = atoi(it->second.c_str());
  return n;
}

const std::string& HTTPClient::FindInMime(const std::map<std::string, std::string>& mime, const std::string& to_find) {
  static std::string empty;
  std::map<std::string, std::string>::const_iterator it;
  it = mime.find(to_find);
  if (it == mime.end()) {
    return empty;
  }
  return it->second;
}

int main(int argc, char* argv[]) {
  if (argc < 5) {
    std::cout << "Please supply hostname, port, method, and command" << std::endl;
    return 1;
  }

  HTTPClient client(argv[1], atoi(argv[2]), argv[3], argv[4]);

  client.SendMessage();
  client.ReadContents();

  return 0;
}
