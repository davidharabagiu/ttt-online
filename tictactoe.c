#if !defined(__unix__) && !defined(_WIN32)
#error "Unsupported platform"
#endif

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef __unix__
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#ifdef _WIN32
#include <ctype.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

#define DEFAULT_PORT 6784

typedef enum {
    MIN_MENU_OPTION = '0',

    MENU_OPTION_CONNECT = '1',
    MENU_OPTION_WAIT = '2',
    MENU_OPTION_EXIT = '3',

    MAX_MENU_OPTION = '4'
} menu_option_t;

typedef enum {
    GAME_STATE_RUNNING = 0,
    GAME_STATE_WON = 1,
    GAME_STATE_LOST = 2,
    GAME_STATE_DRAW = 3
} game_state_t;

#ifdef __unix__
typedef int socket_handle_t;
#endif
#ifdef _WIN32
typedef SOCKET socket_handle_t;
#endif 

void run_game(socket_handle_t sockfd, int is_client);
int getchar_raw();
void clear_screen();
int check_socket_valid(socket_handle_t socket);
void close_socket(socket_handle_t socket);

int main(int argc, char** argv) {
    srand((unsigned int)time(NULL));

    clear_screen();
    printf("Tic-Tac-Toe\n\n");
    printf("1. Connect to peer\n");
    printf("2. Wait for connections\n");
    printf("3. Exit.\n\n");

    menu_option_t option;
    do {
        option = (menu_option_t)getchar_raw();
    } while (option <= MIN_MENU_OPTION || option >= MAX_MENU_OPTION);
    printf("\n\n");

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Could not initialize winsock. Program cannot continue.\n");
        exit(1);
    }
#endif

    socket_handle_t sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (!check_socket_valid(sockfd))
    {
        printf("Could not initialize socket. Program cannot continue.\n");
        exit(1);
    }

    struct sockaddr_in addr;
    memset(&addr, '0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DEFAULT_PORT);

    if (option == MENU_OPTION_CONNECT) {
        printf("Peer address: ");

        char addr_str[16];
        if (scanf("%s", addr_str) != 1) {
            printf("Unable to read from standard input. Program cannot continue.\n");
            exit(1);
        }
        getchar();

        for (;;) {
            if (inet_pton(AF_INET, addr_str, &addr.sin_addr) != 1) {
                printf("Invalid address. Please try again.\n\n");
            }
            else if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
                printf("Could not establish a connection to %s. Please try again.\n\n", addr_str);
            }
            else {
                break;
            }
            printf("Peer address: ");
            if (scanf("%s", addr_str) != 1) {
                printf("Unable to read from standard input. Program cannot continue.\n");
                exit(1);
            }
            getchar();
        }

        run_game(sockfd, 1);

        close_socket(sockfd);
    }
    else if (option == MENU_OPTION_WAIT) {
        socket_handle_t listenfd = sockfd;

        int reuse_port = 1;
        if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (void*)&reuse_port, sizeof(reuse_port)) != 0) {
            printf("Could not set socket options. Program cannot continue.\n");
            exit(1);
        }

        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
            printf("Could not bind address to socket. Program cannot continue.\n");
            exit(1);
        }

        if (listen(listenfd, 1) != 0) {
            printf("Could not listen for connections. Program cannot continue.\n");
            exit(1);
        }

        printf("Waiting for connection...\n");
        sockfd = accept(listenfd, NULL, NULL);
        if (!check_socket_valid(sockfd)) {
            printf("Could not listen for connections. Program cannot continue.\n");
            exit(1);
        }

        run_game(sockfd, 0);

        close_socket(sockfd);
        close_socket(listenfd);
    }

    return 0;
}

void run_game(socket_handle_t sockfd, int is_client) {
    char turn, turn2;
    do {
        turn = (rand() % 2) ? 'X' : '0';
        if (is_client) {
            send(sockfd, &turn, sizeof(turn), 0);
            recv(sockfd, &turn2, sizeof(turn2), 0);
        }
        else {
            recv(sockfd, &turn2, sizeof(turn2), 0);
            send(sockfd, &turn, sizeof(turn), 0);
        }
    } while (turn == turn2);

    char current_turn = 'X';
    char table[3][3];
    int coords[2];
    char ack;

    for (int y = 0; y != 3; ++y) {
        for (int x = 0; x != 3; ++x) {
            table[y][x] = ' ';
        }
    }

    char error[1024];
    error[0] = 0;

    game_state_t game_state = GAME_STATE_RUNNING;

    while (game_state == GAME_STATE_RUNNING) {
        clear_screen();

        if (error[0] != 0) {
            printf("%s\n\n", error);
        }
        else if (turn == current_turn) {
            printf("Your turn\n\n");
        }
        else {
            printf("Opponent's turn\n\n");
        }

        for (int y = 0; y != 3; ++y) {
            printf(" %c | %c | %c\n", table[y][0], table[y][1], table[y][2]);
            if (y < 2) {
                printf("-----------\n");
            }
        }

        if (turn == current_turn) {
            printf("\nRow (1/2/3): ");
            coords[0] = getchar_raw() - '1';
            if (coords[0] < 0 || coords[0] > 2) {
                strcpy(error, "Invalid row number. Try again!");
                continue;
            }

            printf("\nCol (1/2/3): ");
            coords[1] = getchar_raw() - '1';
            if (coords[1] < 0 || coords[1] > 2) {
                strcpy(error, "Invalid column number. Try again!");
                continue;
            }

            if (table[coords[0]][coords[1]] != ' ') {
                strcpy(error, "Invalid coordinates. Try again!");
                continue;
            }

            table[coords[0]][coords[1]] = turn;

            if (is_client) {
                send(sockfd, (void*)coords, sizeof(coords), 0);
                recv(sockfd, &ack, sizeof(ack), 0);
                if (ack != 'A') {
                    printf("Protocol error. Program cannot continue.\n");
                    exit(1);
                }
                ack = 0;
            }
            else {
                recv(sockfd, &ack, sizeof(ack), 0);
                if (ack != 'A') {
                    printf("Protocol error. Program cannot continue.\n");
                    exit(1);
                }
                ack = 0;
                send(sockfd, (void*)coords, sizeof(coords), 0);
            }
        }
        else {
            printf("\nWaiting for opponent...\n");

            if (is_client) {
                ack = 'A';
                send(sockfd, &ack, sizeof(ack), 0);
                ack = 0;
                recv(sockfd, (void*)coords, sizeof(coords), 0);
                table[coords[0]][coords[1]] = current_turn;
            }
            else {
                recv(sockfd, (void*)coords, sizeof(coords), 0);
                table[coords[0]][coords[1]] = current_turn;
                ack = 'A';
                send(sockfd, &ack, sizeof(ack), 0);
                ack = 0;
            }
        }

        current_turn = (current_turn == 'X') ? '0' : 'X';
        error[0] = 0;

        char winner = 0;
        if (table[0][0] != ' ' && table[0][0] == table[0][1] && table[0][1] == table[0][2] && table[0][2] == table[0][0]) {
            winner = table[0][0];
        }
        else if (table[1][0] != ' ' && table[1][0] == table[1][1] && table[1][1] == table[1][2] && table[1][2] == table[1][0]) {
            winner = table[1][0];
        }
        else if (table[2][0] != ' ' && table[2][0] == table[2][1] && table[2][1] == table[2][2] && table[2][2] == table[2][0]) {
            winner = table[2][0];
        }
        else if (table[0][0] != ' ' && table[0][0] == table[1][0] && table[1][0] == table[2][0] && table[2][0] == table[0][0]) {
            winner = table[0][0];
        }
        else if (table[0][1] != ' ' && table[0][1] == table[1][1] && table[1][1] == table[2][1] && table[2][1] == table[0][1]) {
            winner = table[0][1];
        }
        else if (table[0][2] != ' ' && table[0][2] == table[1][2] && table[1][2] == table[2][2] && table[2][2] == table[0][2]) {
            winner = table[0][2];
        }
        else if (table[0][0] != ' ' && table[0][0] == table[1][1] && table[1][1] == table[2][2] && table[2][2] == table[0][0]) {
            winner = table[0][0];
        }
        else if (table[0][2] != ' ' && table[0][2] == table[1][1] && table[1][1] == table[2][0] && table[2][0] == table[0][2]) {
            winner = table[0][2];
        }

        if (winner != 0 && winner == turn) {
            game_state = GAME_STATE_WON;
            continue;
        }
        else if (winner != 0 && winner != turn) {
            game_state = GAME_STATE_LOST;
            continue;
        }

        int draw = 1;
        for (int y = 0; y != 3 && draw; ++y) {
            for (int x = 0; x != 3 && draw; ++x) {
                if (table[y][x] == ' ') {
                    draw = 0;
                }
            }
        }
        if (draw) {
            game_state = GAME_STATE_DRAW;
        }
    }

    if (game_state == GAME_STATE_WON) {
        printf("\nCongratulations! You won!\n");
    }
    else if (game_state == GAME_STATE_LOST) {
        printf("\nNext time, loser!\n");
    }
    else if (game_state == GAME_STATE_DRAW) {
        printf("\nIt's a draw. Boring...\n");
    }
}

int getchar_raw() {
    int c = 0;
#ifdef __unix__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
    system("stty raw");
    c = getchar();
    system("stty cooked");
#pragma GCC diagnostic pop
#endif
#ifdef _WIN32
    // Not using raw input for WIN32
    do {
        c = getchar();
    } while (isspace(c));
#endif
    return c;
}

void clear_screen() {
#ifdef __unix__
    printf("\e[2J\e[H");
#endif
#ifdef _WIN32
    system("cls");
#endif
}

int check_socket_valid(socket_handle_t socket) {
#ifdef __unix__
    return socket >= 0;
#endif
#ifdef _WIN32
    return socket != INVALID_SOCKET;
#endif
}

void close_socket(socket_handle_t socket) {
#ifdef __unix__
    close(socket);
#endif
#ifdef _WIN32
    closesocket(socket);
#endif
}
