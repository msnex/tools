#include <hiredis/hiredis.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_usage(const char *prog_name) {
  fprintf(stderr,
          "Usage:\n"
          "  %s -h <host> -p <port> <command>\n"
          "  %s -s <socket> <command>\n"
          "Examples:\n"
          "  %s -h 127.0.0.1 -p 6379 SET\n"
          "  %s -s /tmp/redis.sock LPUSH\n",
          prog_name, prog_name, prog_name, prog_name);
}

int main(int argc, char **argv) {
  char *host = NULL, *socket_path = NULL, *command = NULL;
  int port = 6379, opt;

  while ((opt = getopt(argc, argv, "h:p:s:")) != -1) {
    if (opt == -1)
      break;
    switch (opt) {
    case 'h':
      host = optarg;
      break;
    case 'p':
      port = atoi(optarg);
      break;
    case 's':
      socket_path = optarg;
      break;
    default:
      print_usage(argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  if (optind >= argc) {
    fprintf(stderr, "Error: Missing Redis command\n");
    print_usage(argv[0]);
    exit(EXIT_FAILURE);
  }
  command = argv[optind];

  if ((host && socket_path) || (!host && !socket_path)) {
    fprintf(stderr, "Error: Must specify either host/port or unix socket\n");
    print_usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  redisContext *conn =
      socket_path ? redisConnectUnix(socket_path) : redisConnect(host, port);

  if (conn->err) {
    fprintf(stderr, "Connection error: %s\n", conn->errstr);
    redisFree(conn);
    exit(EXIT_FAILURE);
  }
  printf("Connected to Redis. Use TAB separated key-value pairs.\n");

  char prompt[32];
  snprintf(prompt, sizeof(prompt), "%s> ", command);
  rl_bind_key('\t', rl_tab_insert);

  while (1) {
    char *input = readline(prompt);
    if (!input) {
      break;
    }

    if (*input == '\0') {
      free(input);
      continue;
    }

    if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
      free(input);
      break;
    }

    char *tab = strchr(input, '\t');
    if (!tab) {
      fprintf(stderr, "Error: use TAB separated key-value pairs.\n");
      free(input);
      continue;
    }
    *tab = '\0';
    char *key = input;
    char *value = tab + 1;

    redisReply *reply = redisCommand(conn, "%s %s %s", command, key, value);
    if (!reply) {
      fprintf(stderr, "Redis error: %s\n", conn->errstr);
      free(input);
      continue;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
      fprintf(stderr, "Redis connection error: %s\n", reply->str);
    }

    freeReplyObject(reply);
    free(input);
  }

  redisFree(conn);
  printf("\nDisconnected from Redis\n");
  return 0;
}
