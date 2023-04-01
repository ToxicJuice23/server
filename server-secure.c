#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <pthread.h>
#define __dirname "/home/jujur/coding/c/server/public"

void send_404(SSL* ssl) {
    char* message = "HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<html>"
"<head><title>404 Not Found</title></head>"
"<center><h1>404 Not Found</h1></center>"
"</body>"
"</html>";
    SSL_write(ssl, message, strlen(message));
    SSL_shutdown(ssl);
    SSL_free(ssl);
}

void send_400(SSL* ssl) {
    char* message = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<html>"
"<head><title>400 Bad Request</title></head>"
"<center><h1>400 Bad Request</h1></center>"
"</body>"
"</html>";
    SSL_write(ssl, message, strlen(message));
    SSL_shutdown(ssl);
    SSL_free(ssl);
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

void serve_resource(SSL* ssl, char* filename) {
    char fn[100];
    sprintf(fn, "%s%s", __dirname, filename);
    if (strstr(fn, "..")) {
        printf("Bad request\n");
        send_400(ssl);
        return;
    }
    FILE* fp = fopen(fn, "r");
    if (!fp) {
        send_404(ssl);
        printf("sent 404\n");
        return;
    }
    fseek(fp, 0L, SEEK_END);
    int sz = ftell(fp);
    rewind(fp);
    char fb[5000000];
    char header[400];
    fread(fb, sz, 1, fp);

    char* content_type = get_content_type(filename);
    sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nConnection: close\r\n\r\n",
    content_type);

    SSL_write(ssl, header, strlen(header));
    int bs = SSL_write(ssl, fb, sz);
    SSL_shutdown(ssl);
    SSL_free(ssl);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: sudo ./server-secure port\n");
        exit(1);
    }
    // init ctx
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM);
    // server code
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    struct addrinfo* bindaddr;
    getaddrinfo(0, argv[1], &hints, &bindaddr);
    // create socket
    int socket_listen = socket(bindaddr->ai_family, bindaddr->ai_socktype, bindaddr->ai_protocol);
    printf("Created socket\n");
    if (bind(socket_listen, bindaddr->ai_addr, bindaddr->ai_addrlen)) {
        printf("Bind() failed. %s\n", strerror(errno));
        exit(1);
    }
    listen(socket_listen, 10);
    // main loop
    while (1) {
        struct sockaddr_storage* clientaddr;
        socklen_t clientlen = sizeof(clientaddr);
        int client = accept(socket_listen, &clientaddr, &clientlen);
        if (!client) {
            printf("Invalid socket\n");
            exit(1);
        }
        char name_buffer[100];
        getnameinfo(&clientaddr, clientlen, name_buffer, 100, 0, 0, NI_NUMERICHOST);
        // create ssl instance
        SSL* ssl = SSL_new(ctx);
        SSL_set_fd(ssl, client);
        SSL_accept(ssl);
        if (!ssl || ssl == NULL) {
            printf("SSL connection failed\n");
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(client);
            continue;
        }
        // handle client in new thread
        int pid = fork();
        if (pid == 0) {
            char request[4096];
            int br = SSL_read(ssl, request, 4096);
            sprintf(request, "%.*s", br, request);
            if (!strstr(request, "GET ") && !strstr(request, "POST ")) {
                printf("Invalid request\n");
                send_400(ssl);
                continue;
            } else if (strstr(request, "POST ")) {
                printf("Post request\n");
                char* body = strstr(request, "\r\n\r\n");
                body += 4;
                printf("Request body is %s\n", body);

                char* response = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/plain\r\n\r\n";
                SSL_write(ssl, response, strlen(response));
                SSL_write(ssl, body, strlen(body));
                SSL_shutdown(ssl);
                SSL_free(ssl);
                close(client);
                continue;
            }
            // analize path
            char* p = request;
            p += 4;
            char* q = strstr(p, " ");
            *q = 0;
            time_t timer;
            time(&timer);
            char* t = ctime(&timer);
            t[strlen(t) - 1] = 0;
            printf("%s: Request from %s at path %s using cipher %s\n", t, name_buffer, p, SSL_get_cipher(ssl));
            if (strstr(p, "//")) {
                printf("Devious request.\n");
                send_400(ssl);
                continue;
            }
            if (strcmp(p, "/") == 0) {p = "/index.html";}
            char filename[200] = __dirname;
            strcat(filename, p);
            if (access(filename, F_OK) != 0) {
                send_404(ssl);
                printf("Sent 404\n");
                return 1;
                continue;
            }
            if (strstr(filename, "..")) {
                send_400(ssl);
                continue;
            }
            // send resource to the client
            serve_resource(ssl, p);
            close(client);
        }
    }
}