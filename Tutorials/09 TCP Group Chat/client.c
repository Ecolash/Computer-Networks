#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <termios.h>
#include <sys/ioctl.h>

const int PORT = 4032;
const int SIZE = 1024;
const int MAX_USERNAME = 50;
const int INPUT_SIZE = 1024;
const int HISTORY_SIZE = 5000;

typedef struct sockaddr *SPTR;

struct termios orig_termios;

char username[50];
char input_buffer[1024] = {0};
int message_alignment[5000] = {0}; 
char message_history[5000][1024];

int _INPUTX_ = 0;
int _MSGCNT_ = 0;
int _ROWS_ = 24;
int _COLS_ = 80;

void timestamp(char *timestamp)
{
    time_t now = time(NULL);
    strftime(timestamp, 20, "%I:%M %p", localtime(&now));
}

void get_terminal_size()
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    _ROWS_ = w.ws_row;
    _COLS_ = w.ws_col;
}

void disable_raw_mode()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode()
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void clear_screen()
{
    printf("\e[2J");
    printf("\e[H");
}

void move_cursor(int row, int col)
{
    printf("\e[%d;%dH", row, col);
    fflush(stdout);
}

void input()
{
    move_cursor(_ROWS_ - 1, 1);
    printf("\e[1;37m>>> \e[0m%s", input_buffer);
    printf("\e[K");
    fflush(stdout);
}

void redraw_screen()
{
    clear_screen();
    move_cursor(1, 1);

    int start = (_MSGCNT_ > _ROWS_ - 3) ? _MSGCNT_ - (_ROWS_ - 3) : 0;
    for (int i = start; i < _MSGCNT_; i++)
    {
        int padding = (message_alignment[i] == 1) ? _COLS_ - strlen(message_history[i]) : 0;
        if (padding < 0) padding = 0;
        printf("%*s%s\n", padding, "", message_history[i]);
    }

    input();
    move_cursor(_ROWS_ - 1, 10 + _INPUTX_);
}

void push(const char *message, int align_right)
{
    if (_MSGCNT_ < HISTORY_SIZE)
    {
        strncpy(message_history[_MSGCNT_], message, SIZE - 1);
        message_alignment[_MSGCNT_] = align_right;
        _MSGCNT_++;
    }
    else
    {
        for (int i = 0; i < HISTORY_SIZE - 1; i++)
        {
            strcpy(message_history[i], message_history[i + 1]);
            message_alignment[i] = message_alignment[i + 1];
        }
        strncpy(message_history[HISTORY_SIZE - 1], message, SIZE - 1);
        message_alignment[HISTORY_SIZE - 1] = align_right;
    }
    redraw_screen();
}

void welcome()
{
    clear_screen();
    int padding = (_COLS_ - 50) / 2; 
    printf("\n");
    printf("%*s╔══════════════════════════════════════════════════╗\n", padding, "");
    printf("%*s║                Welcome to ChatRoom               ║\n", padding, "");
    printf("%*s╚══════════════════════════════════════════════════╝\n\n", padding, "");
}

int main(int argc, char *argv[])
{
    int sfd;
    char recv_buffer[SIZE];

    welcome();
    printf("\e[1mEnter your username:\e[0m ");
    fgets(username, MAX_USERNAME, stdin);
    username[strcspn(username, "\n")] = 0;

    struct sockaddr_in server;
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) { printf("Socket Creation failed!\n"); exit(1); }

    bzero(&server, sizeof(server));
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(PORT);
    server.sin_family = AF_INET;

    printf("Connecting to server...\n");
    sleep(1);
    if (connect(sfd, (SPTR)&server, sizeof(server)) == -1) { printf("Connection failed!\n"); exit(1); }

    send(sfd, username, strlen(username), 0);
    enable_raw_mode();
    get_terminal_size();
    clear_screen();

    char connected_msg[SIZE];
    sprintf(connected_msg, "\e[1;34mConnected as: \e[1;32m\e[1m%s\e[0m\n", username);
    push(connected_msg, 0);
    push("\e[1mGeneral Instructions:\e[0m", 0);
    push(" (1) Type your message and press \e[1;32mEnter\e[0m to send", 0);
    push(" (2) Type '\e[1;31m\\EXIT\e[0m' to quit", 0);
    push(" (3) Be respectful and courteous to others", 0);
    push(" (4) Avoid sharing personal information", 0);
    push("", 0);

    fd_set fds;
    int maxfd;

    while (1)
    {
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        FD_SET(sfd, &fds);

        maxfd = (sfd > STDIN_FILENO) ? sfd : STDIN_FILENO;

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        select(maxfd + 1, &fds, NULL, NULL, &timeout);

        if (FD_ISSET(STDIN_FILENO, &fds))
        {
            char c;
            if (read(STDIN_FILENO, &c, 1) == 1)
            {
                if (c == '\n')
                {
                    input_buffer[_INPUTX_] = '\0';

                    if (strlen(input_buffer) > 0)
                    {
                        if (strcmp(input_buffer, "\\EXIT") == 0)
                        {
                            disable_raw_mode();
                            clear_screen();
                            printf("Goodbye!\n");
                            close(sfd);
                            exit(0);
                        }

                        char time[20];
                        timestamp(time);
                        char self_msg[SIZE];
                        sprintf(self_msg, "You: \e[1;32m%s\e[0m \e[1;37m%s\e[0m [%s]", input_buffer, "", time);
                        push(self_msg, 1); 

                        _INPUTX_ = 0;
                        send(sfd, input_buffer, strlen(input_buffer), 0);
                        memset(input_buffer, 0, INPUT_SIZE);
                    }
                }
                else if (c == 127 && _INPUTX_ > 0) input_buffer[--_INPUTX_] = '\0';
                else if (_INPUTX_ < INPUT_SIZE - 1) input_buffer[_INPUTX_++] = c;
                input();
            }
        }

        if (FD_ISSET(sfd, &fds))
        {
            int n = recv(sfd, recv_buffer, SIZE, 0);
            if (n > 0) { recv_buffer[n] = '\0'; push(recv_buffer, 0); }
            else if (n == 0)
            {
                disable_raw_mode();
                clear_screen();
                printf("\nDisconnected from server.\n");
                close(sfd);
                exit(0);
            }
        }

        get_terminal_size();
        input();
    }
}