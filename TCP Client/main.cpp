#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>

#define SERVERIP   "127.0.0.1"
#define SERVERPORT 9000
#define BUFSIZE    1024

int main()
{
        WSADATA wsa;
        SOCKET sock;
        SOCKADDR_IN serveraddr;
        char buf[BUFSIZE];
        int retval;

        // Winsock 초기화
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        {
                printf("WSAStartup failed\n");
                return 1;
        }

        // 소켓 생성
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET)
        {
                printf("socket() failed\n");
                return 1;
        }

        // 서버 주소 설정
        memset(&serveraddr, 0, sizeof(serveraddr));
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
        serveraddr.sin_port = htons(SERVERPORT);

        // 서버 연결
        if (connect(sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr)) == SOCKET_ERROR)
        {
                printf("connect() failed\n");
                closesocket(sock);
                WSACleanup();
                return 1;
        }

        printf("Connected to server.\n");

        while (1)
        {
                printf("Input message (QUIT to exit): ");
                fgets(buf, BUFSIZE, stdin);

                // 개행 제거
                buf[strcspn(buf, "\n")] = 0;

                // 전송
                retval = send(sock, buf, (int)strlen(buf), 0);
                if (retval == SOCKET_ERROR)
                {
                        printf("send() failed\n");
                        break;
                }

                // QUIT 입력 시 서버 종료 유도 후 클라 종료
                if (strcmp(buf, "QUIT") == 0)
                        break;

                // Echo 수신
                retval = recv(sock, buf, BUFSIZE - 1, 0);
                if (retval <= 0)
                {
                        printf("Server closed connection.\n");
                        break;
                }

                buf[retval] = 0;
                printf("Received: %s\n", buf);
        }

        closesocket(sock);
        WSACleanup();
        return 0;
}