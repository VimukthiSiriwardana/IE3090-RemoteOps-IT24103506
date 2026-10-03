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

/*
 * Read current system information from Linux /proc files.
 *
 * CPU load:
 *     /proc/loadavg
 *
 * Memory:
 *     /proc/meminfo
 *
 * Uptime:
 *     /proc/uptime
 */
void get_system_info(double *cpu_load,
                     long *mem_used_mb,
                     long *uptime_sec)
{
    FILE *file;

    double uptime;

    long mem_total_kb;
    long mem_available_kb;

    /* --------------------------------
     * Read CPU load
     * -------------------------------- */
    file = fopen("/proc/loadavg", "r");

    if (file != NULL)
    {
        fscanf(file, "%lf", cpu_load);

        fclose(file);
    }
    else
    {
        *cpu_load = 0.0;
    }

    /* --------------------------------
     * Read memory information
     * -------------------------------- */
    mem_total_kb = 0;
    mem_available_kb = 0;

    file = fopen("/proc/meminfo", "r");

    if (file != NULL)
    {
        char line[256];

        while (fgets(line, sizeof(line), file) != NULL)
        {
            if (sscanf(line,
                       "MemTotal: %ld kB",
                       &mem_total_kb) == 1)
            {
                continue;
            }

            if (sscanf(line,
                       "MemAvailable: %ld kB",
                       &mem_available_kb) == 1)
            {
                continue;
            }
        }

        fclose(file);
    }

    /*
     * Convert used memory from KB to MB.
     */
    *mem_used_mb =
        (mem_total_kb - mem_available_kb) / 1024;

    /* --------------------------------
     * Read system uptime
     * -------------------------------- */
    file = fopen("/proc/uptime", "r");

    if (file != NULL)
    {
        fscanf(file, "%lf", &uptime);

        fclose(file);

        *uptime_sec = (long)uptime;
    }
    else
    {
        *uptime_sec = 0;
    }
}

int main(void)
{
    int server_fd;
    int reuse = 1;

    struct sockaddr_in server_addr;

    printf("RemoteOps Agent starting...\n");

    /* --------------------------------
     * Create TCP socket
     * -------------------------------- */
    server_fd = socket(AF_INET,
                       SOCK_STREAM,
                       0);

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    printf("TCP socket created successfully.\n");

    /* --------------------------------
     * Allow quick port reuse
     * -------------------------------- */
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

    /* --------------------------------
     * Configure server address
     * -------------------------------- */
    memset(&server_addr,
           0,
           sizeof(server_addr));

    server_addr.sin_family = AF_INET;

    server_addr.sin_addr.s_addr =
        INADDR_ANY;

    server_addr.sin_port =
        htons(PORT);

    /* --------------------------------
     * Bind socket to port 9410
     * -------------------------------- */
    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    printf("Socket bound to port %d.\n",
           PORT);

    /* --------------------------------
     * Start listening
     * -------------------------------- */
    if (listen(server_fd, 5) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("RemoteOps Agent is listening on port %d...\n",
           PORT);

    /* --------------------------------
     * Accept Controllers
     * -------------------------------- */
    while (1)
    {
        int client_fd;

        struct sockaddr_in client_addr;

        socklen_t client_len =
            sizeof(client_addr);

        /* Accept a Controller connection */
        client_fd =
            accept(server_fd,
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

        /* Authentication state */
        int authenticated = 0;

        /* Buffer for TCP lines */
        char buffer[BUFFER_SIZE];

        /* --------------------------------
         * Communicate with this Controller
         * -------------------------------- */
        while (1)
        {
            int line_length;

            /* Read one complete line */
            line_length =
                read_line(client_fd,
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

            printf("Received line: %s",
                   buffer);

            /* --------------------------------
             * Authentication
             * -------------------------------- */
            if (!authenticated)
            {
                /*
                 * AUTH must be the first
                 * successful command.
                 */
                if (strncmp(buffer,
                            "AUTH ",
                            5) != 0)
                {
                    const char *response =
                        "ERR 001 AUTH_FAILED SID:6053\n";

                    send(client_fd,
                         response,
                         strlen(response),
                         0);

                    printf("Authentication required.\n");

                    continue;
                }

                /* Extract authentication token */
                char token[BUFFER_SIZE];

                if (sscanf(buffer,
                           "AUTH %1023s",
                           token) != 1)
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

                /* Compare token */
                if (strcmp(token,
                           AUTH_TOKEN) == 0)
                {
                    authenticated = 1;

                    char response[BUFFER_SIZE];

                    snprintf(response,
                             sizeof(response),
                             "OK AUTHENTICATED SID:%s\n",
                             SID);

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

            /* --------------------------------
             * SYSINFO
             * -------------------------------- */
            if (strcmp(buffer,
                       "SYSINFO\n") == 0)
            {
                double cpu_load;

                long mem_used_mb;
                long uptime_sec;

                char response[BUFFER_SIZE];

                /* Read current system information */
                get_system_info(&cpu_load,
                                &mem_used_mb,
                                &uptime_sec);

                /*
                 * Build the required protocol
                 * response.
                 */
                snprintf(response,
                         sizeof(response),
                         "OK SYSINFO %.2f %ld %ld SID:%s\n",
                         cpu_load,
                         mem_used_mb,
                         uptime_sec,
                         SID);

                /* Send response */
                if (send(client_fd,
                         response,
                         strlen(response),
                         0) < 0)
                {
                    perror("send");
                    break;
                }

                printf("SYSINFO response sent.\n");

                continue;
            }

            /* --------------------------------
             * QUIT
             * -------------------------------- */
            if (strcmp(buffer,
                       "QUIT\n") == 0)
            {
                char response[BUFFER_SIZE];

                snprintf(response,
                         sizeof(response),
                         "OK BYE SID:%s\n",
                         SID);

                send(client_fd,
                     response,
                     strlen(response),
                     0);

                printf("Controller requested disconnect.\n");

                break;
            }

            /* --------------------------------
             * Temporary response
             * -------------------------------- */
            {
                char response[BUFFER_SIZE];

                snprintf(response,
                         sizeof(response),
                         "OK SID:%s\n",
                         SID);

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
        }

        /* Close Controller connection */
        close(client_fd);

        printf("Controller connection closed.\n");
    }

    /* Close server socket */
    close(server_fd);

    return 0;
}
