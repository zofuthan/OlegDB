//        DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
//                    Version 2, December 2004
//
// Copyright (C) 2004 Sam Hocevar <sam@hocevar.net>
//
// Everyone is permitted to copy and distribute verbatim or modified
// copies of this license document, and changing it is allowed as long
// as the name is changed.
//
//            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
//   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
//
//  0. You just DO WHAT THE FUCK YOU WANT TO.

// I can't put this in server.h for some reason

#include "server.h"

const char get_response[] = "HTTP/1.1 200 OK\n"
                          "Content-Type: application/json\n"
                          "Content-Length: %zu\n"
                          "Connection: close\n"
                          "\n%s";

const char post_response[] = "HTTP/1.1 200 OK\n"
                          "Content-Type: text/plain\n"
                          "Connection: close\n"
                          "Content-Length: 7\n"
                          "\n"
                          "MUDADA\n";

const char not_found_response[] = "HTTP/1.1 404 Not Found\n"
                          "Status: 404 Not Found\n"
                          "Content-Length: 26\n"
                          "Connection: close\n"
                          "Content-Type: text/plain\n"
                          "\n"
                          "These aren't your ghosts.\n";

static int ol_make_socket(void) {
    int listenfd;
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Error creating socket");
        exit(1);
    }
    return listenfd;
}

int build_request(char *req_buf, size_t req_len, http *request) {
    // TODO: Make sure theres actually a valid URI in the request
    int i;
    int method_len = 0;
    int url_len = 0;

    // Seek the request until a space char and that's our method
    for (i = 0; i < SOCK_RECV_MAX; i++ ) {
        if (req_buf[i] != ' ' && req_buf[i] != '\n') {
            method_len++;
        } else {
            break;
        }
    }
    if (method_len <= 0) {
        printf("[X] Error: Could not parse method.\n");
        return 1;
    }

    request->method_len = method_len; // The length of the method for offsets
    strncpy(request->method, req_buf, method_len);

    for (i = (method_len+1); i < SOCK_RECV_MAX; i++ ) {
        if (req_buf[i] != ' ' && req_buf[i] != '\r' && req_buf[i] != '\n') {
            url_len++;
        } else {
            break;
        }
    }
    if (url_len <= 0) {
        printf("[X] Error: Could not parse URL.\n");
        return 2;
    }
    request->url_len = url_len;
    strncpy(request->url, req_buf + method_len + 1, url_len);

    char *split_key = strtok(request->url, "/");

    if (split_key == NULL) {
        printf("[X] Error: Could not parse Key.\n");
        return 3;
    }
    strncpy(request->key, split_key, strlen(split_key));

    return 0;
}

void ol_server(ol_database *db, int port) {
    int sock, connfd;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t clilen;

    sock = ol_make_socket();

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr))) {
        printf("[X] Error: Could not bind socket.\n");
        exit(1);
    };

    // Fuck you let me rebind it you asshat
    int optVal = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void*) &optVal,
               sizeof(optVal));

    listen(sock, 1024);

    printf("[-] Listening on %d\n", ntohs(servaddr.sin_port));

    while (1) { // 8======D
        char mesg[1000];
        clilen = sizeof(cliaddr);
        connfd = accept(sock, (struct sockaddr *)&cliaddr, &clilen);

        printf("[-] Records: %i\n", db->rcrd_cnt);
        printf("[-] DB: %p\n", db);

        http *request = calloc(1, sizeof(http));

        while (1) {
            char *resp_buf;

            int n;
            n = recvfrom(connfd, mesg, SOCK_RECV_MAX, 0,
                (struct sockaddr *)&cliaddr, &clilen);

            if (build_request(mesg, n, request) > 0) {
                printf("[X] Error: Could not build request.\n");
                sendto(connfd, not_found_response,
                    sizeof(not_found_response), 0, (struct sockaddr *)&cliaddr,
                    sizeof(cliaddr));
                mesg[0] = '\0';
                break;
            }

            printf("\n[-] ------\n");
            printf("[-] Method: %s\n", request->method);
            printf("[-] URL: %s\n", request->url);
            printf("[-] Key: %s\n", request->key);

            if (strncmp(request->method, "GET", 3) == 0) {
                printf("[-] Method is GET.\n");
                ol_val data = ol_unjar(db, request->key);
                printf("[-] Looked for key.\n");

                if (data != NULL) {
                    // Fuck I don't know about the 2 man whatever
                    size_t content_size = strlen(get_response) + strlen((char*)data);
                    resp_buf = malloc(content_size);

                    sprintf(resp_buf, get_response, strlen((char*)data), data);
                    sendto(connfd, resp_buf,
                        strlen(resp_buf), 0, (struct sockaddr *)&cliaddr,
                        sizeof(cliaddr));
                    free(resp_buf);
                    break;
                } else {
                    printf("[X] Value null.\n");
                    sendto(connfd, not_found_response,
                        sizeof(not_found_response), 0, (struct sockaddr *)&cliaddr,
                        sizeof(cliaddr));
                    break;
                }

            } else if (strncmp(request->method, "POST", 4) == 0) {
                printf("[-] Method is POST.\n");
                unsigned char to_insert[] = "compile a shit";
                if (ol_jar(db, request->key, to_insert, strlen((char*)to_insert)) > 0) {
                    printf("[X] Could not insert\n");
                    sendto(connfd, not_found_response,
                        sizeof(not_found_response), 0, (struct sockaddr *)&cliaddr,
                        sizeof(cliaddr));
                    break;
                } else {
                    printf("[ ] Inserted new value for key %s.\n", request->key);
                    printf("[-] Records: %i\n", db->rcrd_cnt);
                    sendto(connfd, post_response,
                        sizeof(post_response), 0, (struct sockaddr *)&cliaddr,
                        sizeof(cliaddr));
                    break;
                }
            } else {
                printf("[X] No matching method.\n");
                sendto(connfd, not_found_response,
                    sizeof(not_found_response), 0, (struct sockaddr *)&cliaddr,
                    sizeof(cliaddr));
                break;
            }
            close(connfd); // BAI
        }
        free(request);
    }
}