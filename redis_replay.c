#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <hiredis/hiredis.h>

void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s -s socket | (-h host -p port) -c cmd -k key -f file\n", prog);
}

int main(int argc, char *argv[]) {
    char *socket_path = NULL;
    char *host = NULL;
    int port = 6389;
    char *cmd = NULL;
    char *key = NULL;
    char *file_path = NULL;
    int opt;

    while ((opt = getopt(argc, argv, "s:h:p:c:k:f:")) != -1) {
        switch (opt) {
        case 's': socket_path = optarg; break;
        case 'h': host = optarg; break;
        case 'p': port = atoi(optarg); break;
        case 'c': cmd = optarg; break;
        case 'k': key = optarg; break;
        case 'f': file_path = optarg; break;
        default: print_usage(argv[0]); exit(EXIT_FAILURE);
        }
    }

    if ((!socket_path && !host) || !cmd || !key || !file_path) {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    redisContext *c = socket_path ? redisConnectUnix(socket_path) : redisConnect(host, port);
    if (c == NULL || c->err) {
        if (c) {
            fprintf(stderr, "Connection error: %s\n", c->errstr);
            redisFree(c);
        } else {
            fprintf(stderr, "Can't allocate redis context\n");
        }
        exit(EXIT_FAILURE);
    }

    FILE *fp = fopen(file_path, "r");
    if (!fp) {
        perror("File open failed");
        redisFree(c);
        exit(EXIT_FAILURE);
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while ((read = getline(&line, &len, fp)) != -1) {
        if (read == 0) {
            continue;
        }

        if (line[read-1] == '\n') {
            line[read-1] = '\0';
            if (read >= 2) {
                if (line[read-2] == '\r') {
                    line[read-2] = '\0';
                }
            }
        }
        size_t line_len = strlen(line);
        if (line_len == 0) {
            continue;
        }

        redisReply *reply = redisCommand(c, "%s %s %b", cmd, key, line, line_len);
        if (!reply) {
            fprintf(stderr, "Command failed: %s\n", c->errstr);
            continue;
        }

        if (reply->type == REDIS_REPLY_ERROR) {
            fprintf(stderr, "Redis error: %s\n", reply->str);
        }

        freeReplyObject(reply);
    }

    free(line);
    fclose(fp);
    redisFree(c);
    return EXIT_SUCCESS;
}
