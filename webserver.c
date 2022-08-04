#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/sendfile.h>

#define BACKLOG 10
#define BUFFER 50000

#define ERR_RET(message) \
    { \
        perror(message); \
        return 1; \
    }

// request and reader
struct monRequest
{
    char team[BUFFER];   // max is 4 chars in a team name(lets hope)
    char body[BUFFER]; // unbounded cuz who cares
};

struct webHeader
{
    char key[BUFFER];
    char value[BUFFER];
};

struct webRequest
{
    char uri[BUFFER];
    char method[BUFFER];
    char version[BUFFER];
    char *body;
    struct webHeader headers[20];//fix needed
};

const char newline[] = "\r\n";
const char twoNewlines[] = "\r\n\r\n";

//functions to send text and file
//gotta figure out how to receive file, as well as multipart

int sendHeader(const char *key, const char *value, const int newFd)
{
    char resp[] = "HTTP/1.0 200 OK\r\n"
                  "Server: webserver-c\r\n";

    if(send(newFd, resp, sizeof resp, 0) == -1)
        ERR_RET("header sending");

    send(newFd, key, strlen(key), 0);
    send(newFd, ":", sizeof ":", 0);
    send(newFd, twoNewLines, sizeof twoNewLines, 0);
    return 0;
}

int sendText(const char *textToSend, const int newFd)
{
    sendHeader("Content-Type", " text/plain", newFd);
    int bytes_sent = send(newFd, textToSend, strlen(textToSend), 0);

    if(bytes_sent == -1)
        ERR_RET("error sending text");

    send(newFd, newline, sizeof newline, 0);
    return 0;
}

int sendFileWeb(const char *filename, const char *contentType, const int newFd)
{
    FILE *fp;
    fp = fopen(filename, "r");

    if(fseek(fp, 0L, SEEK_END) != 0)
        ERR_RET("seeking");

    int sz = ftell(fp);
    char ch[sz];
    rewind(fp);

    if(fread(ch, 1, sz, (FILE *)fp) < 0)
        ERR_RET("reading");

    if(sendHeader("Content-Type", contentType, newFd) != 0)
        return 1;

    if(send(newFd, ch, sz, 0) == -1)
        ERR_RET("sending file");

    return 0;
}

//add other stuff

// one monwocr
int kVParse(const char *token, const char *p1, const char *p2, const int mode)
{
    char broken[BUFFER];
    strcpy(broken, token);
    char *other_token = strtok(broken, ":");

    if(mode == 1)
    {
        strcpy(p1, other_token);
        token += strlen(other_token) + 2;
        strcpy(p2, token);
        return 0;
    }

    strcpy(p1, other_token);

    while (other_token != NULL)
    {
        strcpy(p2, other_token);

        other_token = strtok(NULL, ":");
    }
    return 0;
}

struct webRequest parseWebRequest(const char *total)
{
    struct webRequest returned;
    sscanf(total, "%s %s %s", returned.method, returned.uri, returned.version);

    //get everything after and incuding carriage return
    returned.body = strstr(total, twoNewLines);
    //remove carriage return
    total += strlen(returned.method) + strlen(returned.uri) + strlen(returned.version) + 4;
    total[strlen(total) - strlen(returned.body)] = '\0';
    returned.body += 4;

    struct webHeader headers[20];
    char *context = NULL;
    int lastentry = 0;

    char *token = strtok_r(total, newline, &context);
    while (token != NULL)
    {
        kVParse(token, headers[lastentry].key, headers[lastentry].value, 1);
        lastentry++;
        token = strtok_r(NULL, newline, &context);
    }

    memcpy(returned.headers, headers, sizeof headers);

    return returned;
}

// two mon
int monRead(const struct monRequest reqsArr[20])
{
    // i think i need to use pointers for this and monappend
    FILE *fp;
    char ch[BUFFER];
    char *context = NULL;
    char *token;
    // allocate memory
    fp = fopen("requests.mon", "r");
    fgets(ch, BUFFER, fp);
    token = strtok_r(ch, ";", &context);

    for(int i = 0; token != NULL; ++i)
    {
        kVParse(token, reqsArr[i].team, reqsArr[i].body, 0);
        token = strtok_r(NULL, ";", &context);
    }

    fclose(fp);
    return 0;
}

// redmonbluemon
int monAppend(const struct monRequest req)
{
    char toadd[BUFFER];
    strcpy(toadd, req.team);
    strcat(toadd, ":");
    strcat(toadd, req.body);
    strcat(toadd, ";");
    // how to do this in one line?
    FILE *fp;
    fp = fopen("requests.mon", "a");
    fprintf(fp, toadd);
    fclose(fp);
    return 0;
}

int monDelete(const struct monRequest req)
{
    struct monRequest allReqs[20];
    int removed = 1;
    monRead(allReqs);
    puts(allReqs[0].team);
    fopen("requests.mon", "w+");

    for (int i = 0; strlen(allReqs[i].team) != NULL; i++)
    {
        if (strcmp(allReqs[i].team, req.team) != 0 || strcmp(allReqs[i].body, req.body) != 0)
            monAppend(allReqs[i]);
        else
            removed = 0;
    }

    return removed;
}

// glorified event loop
int main(void)
{
    int sockfd, newFd, status;
    struct addrinfo hints;
    struct addrinfo *servinfo;
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    char msgb[] = "So, we are gamers!";
    char recvb[BUFFER], method[BUFFER], uri[BUFFER], version[BUFFER];
    int lenm, bytes_sent, recieved;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, "4546", &hints, &servinfo) != 0))
        perror(gai_strerror(status));

    if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)
        perror("problem creating socket");

    // sorry joe, adress isnt already in use
    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1)
        perror("error fixing other error");

    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) != 0)
        perror("Problem binding socket");

    // servinfo is not needed anymore
    freeaddrinfo(servinfo);

    if (listen(sockfd, BACKLOG) != 0)
        perror("Problem listening");

    while (1)
    {
        addr_size = sizeof their_addr;
        newFd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        //errorcheck
        lenm = sizeof msgb;
        recieved = recv(newFd, recvb, sizeof recvb, 0);
        //errorcheck
        puts(recvb);
        sscanf(recvb, "%s %s %s", method, uri, version);
        struct webRequest req = parseWebRequest(recvb);
        FILE *fp = fopen("circles.jpeg", "wb");
        fwrite(req.body, recieved, 1, fp);
        fclose(fp);

        if (strcmp(uri, "/") == 0)
            sendText("Hello, my fellow internet users!", newFd);
        else if (strcmp(uri, "/vader") == 0)
        {
            char msgd[] = "James earl Jones sent for me";
            lenm = sizeof msgd;
            bytes_sent = send(newFd, msgd, lenm, 0);
        }
        else if (strcmp(req.uri, "/http") == 0)
            sendFileWeb("Dance_dance_dance_by_chic_edited_version_US_single_Atlantic.png", " image/jpeg", newFd);
        else
        {
            char notFound[] = "404";
            bytes_sent = send(newFd, notFound, sizeof notFound, 0);
        }

        close(newFd);
    }

    // of the above should be error checked, returns 0/1 if no problem

    return 0;
}
