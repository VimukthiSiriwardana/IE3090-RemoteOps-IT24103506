#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 9410
#define BUFFER_SIZE 1024

#define AUTH_TOKEN "OPS-3506"
#define SID "6053"

/*
 * Read one complete line from a TCP connection.
 *
 * Returns:
 *   > 0  number of characters read
 *   = 0  Controller disconnected
 *   < 0  error
 */
int read_line(int client_fd, char *buffer, size_t buffer_size)
{
    size_t position = 0;

    while (position < buffer_size - 1)
    {
        char ch;
        int bytes_received;

        bytes_received = recv(client_fd, &ch, 1, 0);

        if (bytes_received < 0)
        {
            perror("recv");
            return -1;
        }

        if (bytes_received == 0)
        {
            return 0;
        }

        buffer[position++] = ch;

        if (ch == '\n')
        {
            break;
        }
    }

    buffer[position] = '\0';

    return (int)position;
}

int main(void)
{
    int server_fd;
    int reuse = 1;
    struct sockaddr_in server_addr;

    printf("RemoteOps Agent starting...\n");

    /* Create TCP socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    printf("TCP socket created successfully.\n");

    /* Allow reuse of the server port */
    if (setsockopt(server_fd,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &reuse,
                   sizeof(reuse)) < 0)
    {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    /* Configure server address */
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    /* Bind socket to port 9410 */
    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    printf("Socket bound to port %d.\n", PORT);

    /* Put socket into listening mode */
    if (listen(server_fd, 5) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("RemoteOps Agent is listening on port %d...\n", PORT);

    /* Wait for Controllers */
    while (1)
    {
        int client_fd;
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        /* Accept a Controller connection */
        client_fd = accept(server_fd,
                           (struct sockaddr *)&client_addr,
                           &client_len);

        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        printf("Controller connected from %s:%d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        /* Authentication state for this connection */
        int authenticated = 0;

        /* Buffer for TCP lines */
        char buffer[BUFFER_SIZE];

        /* Keep communicating with this Controller */
        while (1)
        {
            /* Read one complete line */
            int line_length = read_line(client_fd,
                                        buffer,
                                        sizeof(buffer));

            if (line_length < 0)
            {
                printf("Error while reading from Controller.\n");
                break;
            }

            if (line_length == 0)
            {
                printf("Controller disconnected.\n");
                break;
            }

            printf("Received line: %s", buffer);

            /*
             * AUTH must be the first command.
             */
            if (!authenticated)
            {
                /* Check that the command starts with AUTH */
                if (strncmp(buffer, "AUTH ", 5) != 0)
                {
                    const char *response =
                        "ERR 001 AUTH_FAILED SID:6053\n";

                    send(client_fd,
                         response,
                         strlen(response),
                         0);

                    printf("Authentication required.\n");

                    /*
                     * The connection remains open so the
                     * Controller can try AUTH.
                     */
                    continue;
                }

                /* Extract token after "AUTH " */
                char token[BUFFER_SIZE];

                if (sscanf(buffer, "AUTH %1023s", token) != 1)
                {
                    const char *response =
                        "ERR 001 AUTH_FAILED SID:6053\n";

                    send(client_fd,
                         response,
                         strlen(response),
                         0);

                    printf("Authentication failed.\n");
                    continue;
                }

                /* Compare supplied token with personalised token */
                if (strcmp(token, AUTH_TOKEN) == 0)
                {
                    authenticated = 1;

                    const char *response =
                        "OK AUTHENTICATED SID:6053\n";

                    send(client_fd,
                         response,
                         strlen(response),
                         0);

                    printf("Controller authenticated successfully.\n");
                }
                else
                {
                    const char *response =
                        "ERR 001 AUTH_FAILED SID:6053\n";

                    send(client_fd,
                         response,
                         strlen(response),
                         0);

                    printf("Authentication failed.\n");
                }

                continue;
            }

            /*
             * At this point authentication has succeeded.
             *
             * Other RemoteOps commands will be added here
             * in the next stages.
             */

            if (strcmp(buffer, "QUIT\n") == 0)
            {
                const char *response =
                    "OK BYE SID:6053\n";

                send(client_fd,
                     response,
                     strlen(response),
                     0);

                printf("Controller requested disconnect.\n");
                break;
            }

            /* Temporary response for authenticated commands */
            const char *response =
                "OK SID:6053\n";

            if (send(client_fd,
                     response,
                     strlen(response),
                     0) < 0)
            {
                perror("send");
                break;
            }

            printf("Response sent successfully.\n");
        }

        /* Close Controller connection */
        close(client_fd);

        printf("Controller connection closed.\n");
    }

    /* Close server socket */
    close(server_fd);

    return 0;
}
