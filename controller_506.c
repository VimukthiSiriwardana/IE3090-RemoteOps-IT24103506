#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9410
#define AUTH_TOKEN "OPS-3506"

#define BUFFER_SIZE 4096
#define RESPONSE_SIZE 65536

/*
 * Receive one complete line-based protocol response.
 *
 * TCP is a byte stream, so one response may arrive through
 * multiple recv() calls. Continue receiving until '\n' is found.
 */
int receive_line(int sock, char *buffer, size_t buffer_size)
{
    size_t total = 0;

    while (total < buffer_size - 1)
    {
        char ch;

        ssize_t received = recv(sock, &ch, 1, 0);

        if (received < 0)
        {
            perror("recv");
            return -1;
        }

        if (received == 0)
        {
            return 0;
        }

        buffer[total++] = ch;

        if (ch == '\n')
        {
            break;
        }
    }

    buffer[total] = '\0';

    return (int)total;
}

/*
 * Send exactly length bytes.
 */
int send_all(int sock, const void *data, size_t length)
{
    size_t total_sent = 0;

    while (total_sent < length)
    {
        ssize_t sent = send(sock,
                            (const char *)data + total_sent,
                            length - total_sent,
                            0);

        if (sent < 0)
        {
            perror("send");
            return -1;
        }

        if (sent == 0)
        {
            return -1;
        }

        total_sent += (size_t)sent;
    }

    return 0;
}

/*
 * Receive exactly length bytes.
 *
 * This is required for GET because the Agent sends a header
 * followed immediately by exactly filesize raw bytes.
 */
int receive_exact(int sock, void *data, size_t length)
{
    size_t total_received = 0;

    while (total_received < length)
    {
        ssize_t received = recv(sock,
                                (char *)data + total_received,
                                length - total_received,
                                0);

        if (received < 0)
        {
            perror("recv");
            return -1;
        }

        if (received == 0)
        {
            printf("Connection closed before all file bytes were received.\n");
            return -1;
        }

        total_received += (size_t)received;
    }

    return 0;
}

/*
 * PUT a local file to the RemoteOps Agent.
 *
 * Protocol:
 *
 * PUT filename filesize\n
 * <exactly filesize raw bytes>
 *
 * Then receive the Agent's line response.
 */
int put_file(int sock, const char *local_filename)
{
    FILE *file;
    struct stat file_info;

    char command[BUFFER_SIZE];
    char response[RESPONSE_SIZE];

    if (stat(local_filename, &file_info) < 0)
    {
        perror("stat");
        return -1;
    }

    if (!S_ISREG(file_info.st_mode))
    {
        printf("PUT error: '%s' is not a regular file.\n",
               local_filename);
        return -1;
    }

    const char *filename = strrchr(local_filename, '/');

    if (filename != NULL)
    {
        filename++;
    }
    else
    {
        filename = local_filename;
    }

    if (strlen(filename) == 0)
    {
        printf("PUT error: invalid filename.\n");
        return -1;
    }

    file = fopen(local_filename, "rb");

    if (file == NULL)
    {
        perror("fopen");
        return -1;
    }

    int command_length = snprintf(command,
                                  sizeof(command),
                                  "PUT %s %lld\n",
                                  filename,
                                  (long long)file_info.st_size);

    if (command_length < 0 ||
        (size_t)command_length >= sizeof(command))
    {
        printf("PUT error: command is too long.\n");
        fclose(file);
        return -1;
    }

    if (send_all(sock,
                 command,
                 (size_t)command_length) < 0)
    {
        fclose(file);
        return -1;
    }

    char buffer[BUFFER_SIZE];

    long long remaining = (long long)file_info.st_size;

    while (remaining > 0)
    {
        size_t bytes_to_read;

        if (remaining > BUFFER_SIZE)
        {
            bytes_to_read = BUFFER_SIZE;
        }
        else
        {
            bytes_to_read = (size_t)remaining;
        }

        size_t bytes_read = fread(buffer,
                                  1,
                                  bytes_to_read,
                                  file);

        if (bytes_read == 0)
        {
            if (ferror(file))
            {
                perror("fread");
            }
            else
            {
                printf("PUT error: unexpected end of file.\n");
            }

            fclose(file);
            return -1;
        }

        if (send_all(sock,
                     buffer,
                     bytes_read) < 0)
        {
            fclose(file);
            return -1;
        }

        remaining -= (long long)bytes_read;
    }

    fclose(file);

    memset(response, 0, sizeof(response));

    int received = receive_line(sock,
                                response,
                                sizeof(response));

    if (received < 0)
    {
        return -1;
    }

    if (received == 0)
    {
        printf("Agent closed the connection.\n");
        return -1;
    }

    printf("%s", response);

    return 0;
}

/*
 * GET a file from the RemoteOps Agent.
 *
 * Protocol:
 *
 * GET filename\n
 *
 * Agent response:
 *
 * OK FILE_SEND filename filesize SID:6053\n
 *
 * followed immediately by exactly filesize raw bytes.
 *
 * The received bytes are saved as the local filename.
 */
int get_file(int sock, const char *filename)
{
    char command[BUFFER_SIZE];
    char response[RESPONSE_SIZE];

    /*
     * Send GET command.
     */
    int command_length = snprintf(command,
                                  sizeof(command),
                                  "GET %s\n",
                                  filename);

    if (command_length < 0 ||
        (size_t)command_length >= sizeof(command))
    {
        printf("GET error: command is too long.\n");
        return -1;
    }

    if (send_all(sock,
                 command,
                 (size_t)command_length) < 0)
    {
        return -1;
    }

    /*
     * Receive the FILE_SEND header.
     */
    memset(response, 0, sizeof(response));

    int received = receive_line(sock,
                                response,
                                sizeof(response));

    if (received < 0)
    {
        return -1;
    }

    if (received == 0)
    {
        printf("Agent closed the connection.\n");
        return -1;
    }

    printf("%s", response);

    /*
     * Check whether Agent returned an error.
     */
    if (strncmp(response, "OK FILE_SEND ", 13) != 0)
    {
        return 0;
    }

    /*
     * Parse:
     *
     * OK FILE_SEND filename filesize SID:6053
     */
    char received_filename[BUFFER_SIZE];
    long long file_size;
    char sid[64];

    int fields = sscanf(response,
                        "OK FILE_SEND %4095s %lld %63s",
                        received_filename,
                        &file_size,
                        sid);

    if (fields != 3)
    {
        printf("GET error: invalid FILE_SEND header.\n");
        return -1;
    }

    if (file_size < 0)
    {
        printf("GET error: invalid file size.\n");
        return -1;
    }

    /*
     * Open local destination file.
     */
    FILE *file = fopen(filename, "wb");

    if (file == NULL)
    {
        perror("fopen");
        return -1;
    }

    /*
     * Receive exactly file_size bytes.
     */
    char buffer[BUFFER_SIZE];

    long long remaining = file_size;

    while (remaining > 0)
    {
        size_t bytes_to_receive;

        if (remaining > BUFFER_SIZE)
        {
            bytes_to_receive = BUFFER_SIZE;
        }
        else
        {
            bytes_to_receive = (size_t)remaining;
        }

        if (receive_exact(sock,
                          buffer,
                          bytes_to_receive) < 0)
        {
            fclose(file);
            return -1;
        }

        size_t bytes_written = fwrite(buffer,
                                     1,
                                     bytes_to_receive,
                                     file);

        if (bytes_written != bytes_to_receive)
        {
            perror("fwrite");
            fclose(file);
            return -1;
        }

        remaining -= (long long)bytes_to_receive;
    }

    fclose(file);

    printf("GET completed: %s (%lld bytes)\n",
           filename,
           file_size);

    return 0;
}

int main(void)
{
    int sock;
    struct sockaddr_in server_addr;

    char command[BUFFER_SIZE];
    char *response;

    response = malloc(RESPONSE_SIZE);

    if (response == NULL)
    {
        perror("malloc");
        return 1;
    }

    /* Create TCP socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0)
    {
        perror("socket");
        free(response);
        return 1;
    }

    /* Prepare server address */
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET,
                  SERVER_IP,
                  &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(sock);
        free(response);
        return 1;
    }

    /* Connect to Agent */
    if (connect(sock,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sock);
        free(response);
        return 1;
    }

    printf("Connected to RemoteOps Agent.\n");

    /*
     * AUTH must be the first command.
     */
    snprintf(command,
             sizeof(command),
             "AUTH %s\n",
             AUTH_TOKEN);

    if (send_all(sock,
                 command,
                 strlen(command)) < 0)
    {
        close(sock);
        free(response);
        return 1;
    }

    memset(response, 0, RESPONSE_SIZE);

    int received = receive_line(sock,
                                response,
                                RESPONSE_SIZE);

    if (received < 0)
    {
        close(sock);
        free(response);
        return 1;
    }

    if (received == 0)
    {
        printf("Agent closed the connection.\n");
        close(sock);
        free(response);
        return 1;
    }

    printf("Agent response: %s", response);

    /*
     * Interactive command loop.
     */
    while (1)
    {
        printf("\nRemoteOps> ");
        fflush(stdout);

        if (fgets(command,
                  sizeof(command),
                  stdin) == NULL)
        {
            break;
        }

        /* Remove newline from user input */
        command[strcspn(command, "\n")] = '\0';

        /* Ignore empty commands */
        if (strlen(command) == 0)
        {
            continue;
        }

        /*
         * Detect PUT command.
         *
         * Expected:
         *
         * PUT test.txt
         */
        if (strncmp(command, "PUT ", 4) == 0)
        {
            char filename[BUFFER_SIZE];

            if (sscanf(command + 4,
                       "%4095s",
                       filename) != 1)
            {
                printf("Usage: PUT <filename>\n");
                continue;
            }

            if (put_file(sock, filename) < 0)
            {
                printf("PUT failed.\n");
            }

            continue;
        }

        /*
         * Detect GET command.
         *
         * Expected:
         *
         * GET test.txt
         */
        if (strncmp(command, "GET ", 4) == 0)
        {
            char filename[BUFFER_SIZE];

            if (sscanf(command + 4,
                       "%4095s",
                       filename) != 1)
            {
                printf("Usage: GET <filename>\n");
                continue;
            }

            if (get_file(sock, filename) < 0)
            {
                printf("GET failed.\n");
            }

            continue;
        }

        /*
         * Add protocol newline for normal commands.
         */
        size_t length = strlen(command);

        if (length + 1 >= sizeof(command))
        {
            printf("Command is too long.\n");
            continue;
        }

        command[length] = '\n';
        command[length + 1] = '\0';

        /*
         * Send normal command to Agent.
         */
        if (send_all(sock,
                     command,
                     strlen(command)) < 0)
        {
            break;
        }

        /*
         * Receive the complete line-based response.
         */
        memset(response, 0, RESPONSE_SIZE);

        received = receive_line(sock,
                                response,
                                RESPONSE_SIZE);

        if (received < 0)
        {
            break;
        }

        if (received == 0)
        {
            printf("Agent closed the connection.\n");
            break;
        }

        printf("%s", response);

        /*
         * End Controller after QUIT.
         */
        if (strncmp(response,
                    "OK BYE",
                    6) == 0)
        {
            break;
        }
    }

    close(sock);
    free(response);

    printf("Disconnected from RemoteOps Agent.\n");

    return 0;
}
