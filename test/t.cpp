//测试大文件传输是否会出现问题

#include <cstdio>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
using namespace std;

int main() {

    for (int i = 0; i < 999999; i ++) {
        printf("%d", i);
    }

    return 0;
}