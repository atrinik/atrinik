#ifdef WIN32
#   include <winsock2.h>
#else
#   include <sys/socket.h>
#endif

int main(void) {
    struct sockaddr_storage ss;
    ss.ss_family = 0;
    return 0;
}
