#include "/home/jujur/coding/c/socket.h"
#include <stdlib.h>
#define __dirname "/home/jujur/coding/c/example/public"

void send_404(int client) {
    char* message = "HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<html>"
"<head><title>404 Not Found</title></head>"
"<center><h1>404 Not Found</h1></center>"
"</body>"
"</html>";
    send(client, message, strlen(message), 0);
    close(client);
}

void send_400(int client) {
    char* message = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<html>"
"<head><title>400 Bad Request</title></head>"
"<center><h1>400 Bad Request</h1></center>"
"</body>"
"</html>";
    send(client, message, strlen(message), 0);
    close(client);
}

char* get_content_type(const char* path) {
    const char* dot = strrchr(path, '.');
    if(strcmp(dot, ".txt") == 0) return "text/plain";
    if(strcmp(dot, ".csv") == 0) return "text/csv";
    if(strcmp(dot, ".gif") == 0) return "image/gif";
    if(!strcmp(dot, ".htm")) return "text/html";
    if(!strcmp(dot, ".html")) return "text/html";
    if(!strcmp(dot, ".ico")) return "image/x-icon";
    if(!strcmp(dot, ".jpeg")) return "image/jpeg";
    if(!strcmp(dot, ".jpg")) return "image/jpeg";
    if(!strcmp(dot, ".js")) return "application/javascript";
    if(!strcmp(dot, ".json")) return "application/json";
    if(!strcmp(dot, ".png")) return "image/png";
    if(!strcmp(dot, ".pdf")) return "application/pdf";
    if(!strcmp(dot, ".svg")) return "image/svg+xml";
    if(!strcmp(dot, ".css")) return "text/css";
    return "application/octet-stream";
}

void serve_resource(int client, char* filename) {
    char fn[100];
    sprintf(fn, "%s%s", __dirname, filename);
    if (strstr(fn, "..")) {
        printf("Bad request\n");
        send_400(client);
        return;
    }
    FILE* fp = fopen(fn, "r");
    if (!fp) {
        send_404(client);
        printf("sent 404\n");
        return;
    }
    fseek(fp, 0L, SEEK_END);
    int sz = ftell(fp);
    rewind(fp);
    char fb[50000];
    fread(fb, sz, 1, fp);
    send(client, fb, sz, 0);
    close(client);
}

int main(int argc, char** argv) {
    // server code
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    struct addrinfo* bindaddr;
    getaddrinfo(0, argv[1], &hints, &bindaddr);
    int socket_listen = socket(bindaddr->ai_family, bindaddr->ai_socktype, bindaddr->ai_protocol);
    printf("Created socket\n");
    bind(socket_listen, bindaddr->ai_addr, bindaddr->ai_addrlen);
    listen(socket_listen, 10);

    while (1) {
        struct sockaddr_storage* clientaddr;
        socklen_t clientlen = sizeof(clientaddr);
        int client = accept(socket_listen, &clientaddr, &clientlen);
        char name_buffer[100];
        getnameinfo(&clientaddr, clientlen, name_buffer, 100, 0, 0, NI_NUMERICHOST);

        char request[4096];
        int br = recv(client, request, 4096, 0);
        sprintf(request, "%.*s", br, request);
        if (!strstr(request, "GET ")) {
            printf("Invalid request\n");
            send_400(client);
            continue;
        }
            char* p = request;
            p += 4;
            char* q = strstr(p, " ");
            *q = 0;
            printf("Request from %s at path %s\n", name_buffer, p);
            if (strcmp(p, "/") == 0) {p = "/index.html";}
            char* content_type = get_content_type(p);

            char filename[200] = __dirname;
            strcat(filename, p);
            if (!fopen(filename, "r")) {
                send_404(client);
                continue;
            }
            if (strstr(filename, "..")) {
                send_400(client);
                continue;
            }

            char message[300];
            sprintf(message, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nConnection: close\r\n\r\n",
            content_type);
            send(client, message, strlen(message), 0);
            serve_resource(client, p);
    }
}