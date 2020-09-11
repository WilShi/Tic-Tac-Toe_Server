#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#define CLI_NUM 20
#define BUFF_SIZE 2048

char board[9];
fd_set fds;
int maxfd = 1, turn = 0, x = 0, o = 0;

struct clients {
	int clientfd;
    char ip[20];
    char role;
    struct clients *next;
};

struct clients *head = NULL;
extern int sent_msg(int fd, char *msg);
extern char whos_turn();

void showboard(int fd)
{
    char buf[100], *bufp, *boardp;
    int col, row;

    for (bufp = buf, col = 0, boardp = board; col < 3; col++) {
        for (row = 0; row < 3; row++, bufp += 4)
            sprintf(bufp, " %c |", *boardp++);
        bufp -= 2;  // kill last " |"
        strcpy(bufp, "\r\n---+---+---\r\n");
        bufp = strchr(bufp, '\0');
    }
    if (write(fd, buf, bufp - buf) != bufp-buf)
	    perror("write");
}

void init(){
    for (int i = 0; i < sizeof board; i++) // Initialize the board
        board[i] = '1' + i;
}

int game_is_over()  /* returns winner, or ' ' for draw, or 0 for not over */
{
    int i, c;
    extern int allthree(int start, int offset);
    extern int isfull();

    for (i = 0; i < 3; i++)
        if ((c = allthree(i, 3)) || (c = allthree(i * 3, 1)))
            return(c);
    if ((c = allthree(0, 4)) || (c = allthree(2, 2)))
        return(c);
    if (isfull())
        return(' ');
    return(0);
}

int allthree(int start, int offset)
{
    if (board[start] > '9' && board[start] == board[start + offset]
            && board[start] == board[start + offset * 2])
        return(board[start]);
    return(0);
}

int isfull()
{
    int i;
    for (i = 0; i < 9; i++)
        if (board[i] < 'a')
            return(0);
    return(1);
}

int sent_msg(int fd, char *msg){
    struct clients *p;
    for (p = head; p; p = p->next){
        if (p->clientfd != fd){
            if (write(p->clientfd, msg, strlen(msg)) < strlen(msg)){
                perror("write");
                return(1);
            }
        }
    }
    return 0;
}

int new_client(int fd, char *ip){
    struct clients *new;
    char msg[BUFF_SIZE];
    if ((new = malloc(sizeof(struct clients))) == NULL) {
        fprintf(stderr, "out of memory!\n");
        return(1);
    }
    printf("new connection from %s\n", ip);
    showboard(fd);
    sprintf(msg, "It is %c's turn.\r\n", whos_turn());
    if (write(fd, msg, strlen(msg)) < strlen(msg)){
        perror("write");
        return 1;
    }
    new->clientfd = fd;
    strcpy(new->ip, ip);

    if (x == 0){
        new->role = 'x';
        x = 1;
        strcpy(msg, "You now get to play!  You are now x.\r\n");
        if (write(fd, msg, strlen(msg)) < strlen(msg)){
            perror("write");
            return(1);
        }
        sprintf(msg, "%s is now playing \'x\'.\r\n", ip);
        sent_msg(fd, msg);
        printf("client from %s is now \'x\'\n", ip);
    }
    else if (x == 1 && o == 0){
        new->role = 'o';
        o = 1;
        strcpy(msg, "You now get to play!  You are now o.\r\n");
        if (write(fd, msg, strlen(msg)) < strlen(msg)){
            perror("write");
            return(1);
        }
        sprintf(msg, "%s is now playing \'o\'.\r\n", ip);
        sent_msg(fd, msg);
        printf("client from %s is now \'o\'\n", ip);
    }
    else if (x == 1 && o == 1){
        new->role = 0;
    }
    
    new->next = head;
    head = new;
    return 0;
}

int delete_client(int fd)
{
    struct clients **pp;
    for (pp = &head; *pp && (*pp)->clientfd != fd; pp = &(*pp)->next)
        ;

    if (*pp && (*pp)->clientfd == fd) {
        struct clients *next = (*pp)->next;
        free(*pp);
        *pp = next;
    }
    return 0;
}

void play_game(char role, int pos){
    struct clients *p;
    char msg[BUFF_SIZE];
    sprintf(msg, "%c make move %d\r\n", role, pos);
    sent_msg(0, msg);
    board[pos-1] = role;
    turn++;
    for (p = head; p; p = p->next){
        showboard(p->clientfd);
        if (whos_turn() == p->role){
            msg[0] = '\0';
            strcpy(msg, "It is your turn.\r\n");
        }
        else{
            msg[0] = '\0';
            sprintf(msg, "It is %c's turn.\r\n", whos_turn());
        }
        if (write(p->clientfd, msg, strlen(msg)) < strlen(msg))
            perror("write");
    }
    if (game_is_over() != 0){
        msg[0] = '\0';
        turn = 0;
        strcpy(msg, "Game over!\r\n");
        sent_msg(0, msg);
        for (p = head; p; p = p->next){
            showboard(p->clientfd);
            msg[0] = '\0';
            if (game_is_over() != ' '){
                sprintf(msg, "%c wins.\r\n", game_is_over());
            }
            else{
                strcpy(msg, "It is a draw.\r\n");
            }
            if (write(p->clientfd, msg, strlen(msg)) < strlen(msg))
                perror("write");
        }
        printf("%s", msg);
        init();
        for (p = head; p; p = p->next){
            msg[0] = '\0';
            strcpy(msg, "Let's play again!\r\n");
            if (write(p->clientfd, msg, strlen(msg)) < strlen(msg))
                perror("write");
                
            msg[0] = '\0';
            if (p->role == 'x'){
                p->role = 'o';
                strcat(msg, "You are now o.\r\n");
                printf("client from %s is now \'o\'\n", p->ip);
            }
            else if (p->role == 'o'){
                p->role = 'x';
                strcat(msg, "You are now x.\r\n");
                printf("client from %s is now \'x\'\n", p->ip);
            }
            if (write(p->clientfd, msg, strlen(msg)) < strlen(msg))
                perror("write");
            showboard(p->clientfd);
            sprintf(msg, "It is %c's turn.\r\n", whos_turn());
            if (write(p->clientfd, msg, strlen(msg)) < strlen(msg))
                perror("write");
        }
    }
}

void change_player(int fd, char role){
    char msg[BUFF_SIZE];
    printf("Player exit!\n");
    if (role == 'x')
        x = 0;
    else
        o = 0;
    struct clients *p;
    for (p = head; p; p = p->next){
        if (p->role == 0){
            p->role = role;
            if (role == 'x')
                x = 1;
            else
                o = 1;
            sprintf(msg, "You now get to play!  You are now %c.\r\n", role);
            if (write(p->clientfd, msg, strlen(msg)) < strlen(msg))
                perror("write");
            break;
        }
    }
    if (x == 0 || o == 0){
        printf("Wait for other player join!\n");
        strcpy(msg, "Wait for other player join!\r\n");
        sent_msg(fd, msg);
    }
}

char *extractline(char *p, int size){
    int nl;
    for (nl = 0; nl < size && p[nl] != '\r' && p[nl] != '\n'; nl++)
	;
    if (nl == size)
	    return(NULL);

    if (p[nl] == '\r' && nl + 1 < size && p[nl+1] == '\n') {
        /* CRLF */
        p[nl] = '\0';
        return(p + nl + 2);
    } else {
        /* lone \n or \r */
        p[nl] = '\0';
        return(p + nl + 1);
    }
}

int resv_message(fd_set fds){
    struct clients *p;
    char resv_msg[BUFF_SIZE], msg[BUFF_SIZE], out_msg[BUFF_SIZE];
    int len;
    for (p = head; p; p = p->next){
        if (FD_ISSET(p->clientfd, &fds)){
            resv_msg[0] = '\0';
            msg[0] = '\0';

            if ((len = read(p->clientfd, resv_msg, BUFF_SIZE-1)) < 0){
                perror("read");
                return(1);
            }

            else if (len > 0){
                resv_msg[len] = '\0';
                strcpy(msg, resv_msg);
                while(extractline(resv_msg, len) == NULL){
                    if ((len = read(p->clientfd, resv_msg, BUFF_SIZE-1)) < 0){
                        perror("read");
                        return(1);
                    }
                    resv_msg[len] = '\0';
                    strcat(msg, resv_msg);
                }

                char *nextpos;
                int size = strlen(msg);
                while ((nextpos = extractline(msg, size))) {
                    if (strlen(msg) == 1 && (isdigit(msg[0]) && msg[0] != '0')){
                        if (whos_turn() == p->role){
                            printf("%s (%c) makes move %c\n", p->ip, p->role, msg[0]);
                            play_game(p->role, msg[0]-'0');
                        }
                        else if (whos_turn() != p->role && (p->role == 'x' || p->role == 'o')){
                            printf("%s tries to make move %c, but it's not their turn\n"
                            , p->ip, msg[0]);

                            strcpy(out_msg, "It is not your turn\r\n");
                            if (write(p->clientfd, out_msg, strlen(out_msg)) < strlen(out_msg)){
                                perror("write");
                                return 1;
                            }
                        }
                        else{
                            printf("%s tries to make move %c, but they aren't playing\n"
                            , p->ip, msg[0]);
                            strcpy(out_msg, "You can't make moves; you can only watch the game\r\n");
                            if (write(p->clientfd, out_msg, strlen(out_msg)) < strlen(out_msg)){
                                perror("write");
                                return 1;
                            }
                        }
                    }
                    else{
                        strcpy(out_msg, "chat message: ");
                        strcat(out_msg, msg);
                        printf("%s\n", out_msg);
                        strcat(out_msg, "\r\n");
                        msg[strlen(out_msg)] = '\0';
                        sent_msg(p->clientfd, out_msg);
                    }
                    size -= nextpos - msg;
                    memmove(msg, nextpos, size);
                }
            }
            else{
                printf("disconnecting client %s\n", p->ip);
                FD_CLR(p->clientfd, &fds);
                if (p->role != 0)
                    change_player(p->clientfd, p->role);
                delete_client(p->clientfd);
                continue;
            }
        }
    }
    return 0;
}

char whos_turn(){
    if ((turn % 2) == 0)
        return 'x';
    return 'o';
}

int main(int argc, char **argv)
{
    int c, port = 3000, status = 0;
    int fd, clientfd;
    socklen_t size;
    struct sockaddr_in ser_addr, cli_addr;
    char ip_addr[20], out_msg[BUFF_SIZE];

    while ((c = getopt(argc, argv, "p:")) != EOF) {
        switch (c) {
        case 'p':
            port = atoi(optarg);
            break;
        default:
            status = 1;
        }
    }

    if (status || optind < argc) {
        fprintf(stderr, "usage: %s [-p port]\n", argv[0]);
        return(1);
    }

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return(1);
    }

    memset(&ser_addr, '\0', sizeof ser_addr);
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = INADDR_ANY;
    ser_addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&ser_addr, sizeof ser_addr) < 0) {
        perror("bind");
        return(1);
    }

    if (listen(fd, CLI_NUM)) {
        perror("listen");
        return(1);
    }
    init();

    while(1){
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        if (maxfd < 0)
            maxfd = 0;

        FD_SET(fd, &fds);
        if (maxfd < fd)
            maxfd = fd;

        struct clients *p;
        for (p = head; p; p = p->next){
            FD_SET(p->clientfd, &fds);
            if (maxfd < p->clientfd)
                maxfd = p->clientfd;
        }

        if (select(maxfd + 1, &fds, NULL, NULL, NULL) < 0){
            perror("select");
            return(1);
        }

        if (FD_ISSET(0, &fds)){
            out_msg[0] = '\0';
            fgets(out_msg, BUFF_SIZE, stdin);
            strcat(out_msg, "\r\n");
            out_msg[strlen(out_msg)-1] = '\0';
            sent_msg(fd, out_msg);
        }

        if (FD_ISSET(fd, &fds)){
            size = sizeof cli_addr;
            if ((clientfd = accept(fd, (struct sockaddr *)&cli_addr, &size)) < 0) {
                perror("accept");
                return(1);
            }

            if (clientfd > 0){
                ip_addr[0] = '\0';
                strcpy(ip_addr, inet_ntoa(cli_addr.sin_addr));
                new_client(clientfd, ip_addr);

            }
        }

        resv_message(fds);
    }
    return(0);
}