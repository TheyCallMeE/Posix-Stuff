#include <stdio.h>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
  std::string test = "Does sscanf\rprocess\nescape?";
  char a[6];
  char b[6];
  char c[6];
  char d[6];

  sscanf(test.c_str(), "%s %s %s %s", a, b, c, d);
  std::cout << a << std::endl;
  std::cout << b << std::endl;
  std::cout << c << std::endl;
  std::cout << d << std::endl;

  return 0;
}
