/*
 * IE3090 Network Programming
 * RemoteOps Controller
 *
 * Registration Number : IT24103506
 * Controller Source   : controller_506.c
 * Agent Port          : 9410
 * Session ID          : SID:6053
 * Authentication     : OPS-3506
 *
 * TCP is used for commands and file transfers.
 * UDP is used for MONITOR SYSINFO datagrams.
 *
 * Optional Extension:
 * Transfer throughput reporting for PUT and GET.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netinet/in.h>

#define AGENT_IP "127.0.0.1"
#define AGENT_PORT 9410

#define AUTH_TOKEN "OPS-3506"
#define SID "6053"

#define BUFFER_SIZE 65536
#define FILENAME_SIZE 256


/*
 * ---------------------------------------------------------
 * Utility: send all bytes
 * ---------------------------------------------------------
 */

static int send_all(
    int socket_fd,
    const void *buffer,
    size_t length
)
{
    const char *data = (const char *)buffer;
    size_t total = 0;

    while (total < length)
    {
        ssize_t sent = send(
            socket_fd,
            data + total,
            length - total,
            0
        );

        if (sent < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            perror("send");
            return -1;
        }

        if (sent == 0)
        {
            return -1;
        }

        total += (size_t)sent;
    }

    return 0;
}


/*
 * ---------------------------------------------------------
 * Utility: receive one complete line
 * ---------------------------------------------------------
 *
 * TCP is a byte stream, so one recv() is NOT assumed to
 * contain one complete response.
 */

static int receive_line(
    int socket_fd,
    char *buffer,
    size_t buffer_size
)
{
    size_t position = 0;

    while (position < buffer_size - 1)
    {
        char character;

        ssize_t received = recv(
            socket_fd,
            &character,
            1,
            0
        );

        if (received < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            perror("recv");
            return -1;
        }

        if (received == 0)
        {
            return 0;
        }

        buffer[position++] = character;

        if (character == '\n')
        {
            buffer[position] = '\0';
            return 1;
        }
    }

    buffer[buffer_size - 1] = '\0';

    fprintf(
        stderr,
        "Response line is too long.\n"
    );

    return -1;
}


/*
 * ---------------------------------------------------------
 * Utility: receive exact number of bytes
 * ---------------------------------------------------------
 */

static int receive_exact(
    int socket_fd,
    void *buffer,
    size_t length
)
{
    char *data = (char *)buffer;
    size_t total = 0;

    while (total < length)
    {
        ssize_t received = recv(
            socket_fd,
            data + total,
            length - total,
            0
        );

        if (received < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            perror("recv");
            return -1;
        }

        if (received == 0)
        {
            fprintf(
                stderr,
                "Connection closed before all expected bytes arrived.\n"
            );

            return -1;
        }

        total += (size_t)received;
    }

    return 0;
}


/*
 * ---------------------------------------------------------
 * Utility: elapsed time
 * ---------------------------------------------------------
 *
 * Returns elapsed time in seconds using a monotonic clock.
 * CLOCK_MONOTONIC is suitable for measuring transfer duration
 * because it is not affected by changes to the system clock.
 */

static double elapsed_seconds(
    const struct timespec *start,
    const struct timespec *end
)
{
    double seconds;
    double nanoseconds;

    seconds = (double)(end->tv_sec - start->tv_sec);
    nanoseconds = (double)(end->tv_nsec - start->tv_nsec);

    return seconds + (nanoseconds / 1000000000.0);
}


/*
 * ---------------------------------------------------------
 * PUT
 * ---------------------------------------------------------
 */

static int put_file(
    int tcp_socket,
    const char *local_filename
)
{
    FILE *file;
    long file_size;
    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    file = fopen(
        local_filename,
        "rb"
    );

    if (file == NULL)
    {
        perror("fopen");
        return -1;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        perror("fseek");
        fclose(file);
        return -1;
    }

    file_size = ftell(file);

    if (file_size < 0)
    {
        perror("ftell");
        fclose(file);
        return -1;
    }

    if (fseek(file, 0, SEEK_SET) != 0)
    {
        perror("fseek");
        fclose(file);
        return -1;
    }

    /*
     * The assignment protocol is:
     *
     * PUT <filename> <filesize>\n
     * followed immediately by raw bytes.
     */

    snprintf(
        command,
        sizeof(command),
        "PUT %s %ld\n",
        local_filename,
        file_size
    );

    printf(
        "Uploading %s (%ld bytes)...\n",
        local_filename,
        file_size
    );

    /*
     * Start throughput timing immediately before the
     * PUT command and file bytes are sent.
     */

    struct timespec start_time;
    struct timespec end_time;

    if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0)
    {
        perror("clock_gettime");
        fclose(file);
        return -1;
    }

    if (send_all(
            tcp_socket,
            command,
            strlen(command)
        ) < 0)
    {
        fclose(file);
        return -1;
    }

    /*
     * Send the exact file bytes.
     */

    char buffer[8192];

    while (1)
    {
        size_t bytes_read = fread(
            buffer,
            1,
            sizeof(buffer),
            file
        );

        if (bytes_read > 0)
        {
            if (send_all(
                    tcp_socket,
                    buffer,
                    bytes_read
                ) < 0)
            {
                fclose(file);
                return -1;
            }
        }

        if (bytes_read < sizeof(buffer))
        {
            if (feof(file))
            {
                break;
            }

            if (ferror(file))
            {
                perror("fread");
                fclose(file);
                return -1;
            }
        }
    }

    fclose(file);

    /*
     * Receive the Agent's response.
     */

    int result = receive_line(
        tcp_socket,
        response,
        sizeof(response)
    );

    if (result <= 0)
    {
        return -1;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &end_time) != 0)
    {
        perror("clock_gettime");
        return -1;
    }

    printf(
        "%s",
        response
    );

    /*
     * Throughput is reported only when the transfer succeeded.
     */

    if (strncmp(
            response,
            "OK FILE_RECEIVED",
            16
        ) == 0)
    {
        double duration = elapsed_seconds(
            &start_time,
            &end_time
        );

        if (duration <= 0.0)
        {
            duration = 0.000001;
        }

        double throughput =
            (double)file_size / duration;

        printf(
            "PUT throughput: %.2f bytes/sec\n",
            throughput
        );
    }

    return 0;
}


/*
 * ---------------------------------------------------------
 * GET
 * ---------------------------------------------------------
 */

static int get_file(
    int tcp_socket,
    const char *remote_filename
)
{
    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    snprintf(
        command,
        sizeof(command),
        "GET %s\n",
        remote_filename
    );

    /*
     * Start timing immediately before sending the GET request.
     */

    struct timespec start_time;
    struct timespec end_time;

    if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0)
    {
        perror("clock_gettime");
        return -1;
    }

    if (send_all(
            tcp_socket,
            command,
            strlen(command)
        ) < 0)
    {
        return -1;
    }

    /*
     * Expected response:
     *
     * OK FILE_SEND <filename> <size> SID:6053
     */

    int result = receive_line(
        tcp_socket,
        response,
        sizeof(response)
    );

    if (result <= 0)
    {
        return -1;
    }

    printf(
        "%s",
        response
    );

    /*
     * If this is not a successful FILE_SEND response,
     * there are no raw file bytes to receive.
     */

    if (strncmp(
            response,
            "OK FILE_SEND ",
            13
        ) != 0)
    {
        return 0;
    }

    char returned_filename[FILENAME_SIZE];
    unsigned long file_size;

    if (sscanf(
            response,
            "OK FILE_SEND %255s %lu SID:%*s",
            returned_filename,
            &file_size
        ) != 2)
    {
        fprintf(
            stderr,
            "Could not parse FILE_SEND response.\n"
        );

        return -1;
    }

    FILE *file = fopen(
        remote_filename,
        "wb"
    );

    if (file == NULL)
    {
        perror("fopen");

        /*
         * We must still consume the exact number of bytes
         * from the TCP connection.
         */

        char discard[8192];
        unsigned long remaining = file_size;

        while (remaining > 0)
        {
            size_t chunk =
                remaining > sizeof(discard)
                    ? sizeof(discard)
                    : (size_t)remaining;

            if (receive_exact(
                    tcp_socket,
                    discard,
                    chunk
                ) < 0)
            {
                return -1;
            }

            remaining -= chunk;
        }

        return -1;
    }

    /*
     * Receive exactly file_size bytes.
     */

    char buffer[8192];
    unsigned long remaining = file_size;

    while (remaining > 0)
    {
        size_t chunk =
            remaining > sizeof(buffer)
                ? sizeof(buffer)
                : (size_t)remaining;

        if (receive_exact(
                tcp_socket,
                buffer,
                chunk
            ) < 0)
        {
            fclose(file);
            return -1;
        }

        size_t written = fwrite(
            buffer,
            1,
            chunk,
            file
        );

        if (written != chunk)
        {
            perror("fwrite");
            fclose(file);
            return -1;
        }

        remaining -= chunk;
    }

    fclose(file);

    /*
     * The complete file has now been received.
     * Stop throughput timing here.
     */

    if (clock_gettime(CLOCK_MONOTONIC, &end_time) != 0)
    {
        perror("clock_gettime");
        return -1;
    }

    printf(
        "GET completed: %s (%lu bytes)\n",
        remote_filename,
        file_size
    );

    double duration = elapsed_seconds(
        &start_time,
        &end_time
    );

    if (duration <= 0.0)
    {
        duration = 0.000001;
    }

    double throughput =
        (double)file_size / duration;

    printf(
        "GET throughput: %.2f bytes/sec\n",
        throughput
    );

    return 0;
}


/*
 * ---------------------------------------------------------
 * UDP monitoring
 * ---------------------------------------------------------
 */

typedef struct
{
    int socket_fd;
    volatile int running;
} monitor_context_t;


/*
 * UDP receiver thread.
 */

static void *monitor_receiver(void *argument)
{
    monitor_context_t *context =
        (monitor_context_t *)argument;

    char buffer[BUFFER_SIZE];

    struct sockaddr_in sender_address;
    socklen_t sender_length =
        sizeof(sender_address);

    while (context->running)
    {
        /*
         * Use select() with a short timeout so the thread
         * can periodically check the running flag.
         */

        fd_set read_set;

        FD_ZERO(&read_set);

        FD_SET(
            context->socket_fd,
            &read_set
        );

        struct timeval timeout;

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ready = select(
            context->socket_fd + 1,
            &read_set,
            NULL,
            NULL,
            &timeout
        );

        if (ready < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            perror("select");
            break;
        }

        if (ready == 0)
        {
            continue;
        }

        if (FD_ISSET(
                context->socket_fd,
                &read_set
            ))
        {
            ssize_t received = recvfrom(
                context->socket_fd,
                buffer,
                sizeof(buffer) - 1,
                0,
                (struct sockaddr *)&sender_address,
                &sender_length
            );

            if (received < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                if (!context->running)
                {
                    break;
                }

                perror("recvfrom");
                break;
            }

            buffer[received] = '\0';

            printf(
                "\nUDP: %s\n",
                buffer
            );

            printf(
                "RemoteOps> "
            );

            fflush(stdout);
        }
    }

    return NULL;
}


/*
 * Create and bind UDP monitoring socket.
 */

static int create_monitor_socket(
    int udp_port
)
{
    int udp_socket;

    struct sockaddr_in address;

    udp_socket = socket(
        AF_INET,
        SOCK_DGRAM,
        0
    );

    if (udp_socket < 0)
    {
        perror("socket UDP");
        return -1;
    }

    int option = 1;

    if (setsockopt(
            udp_socket,
            SOL_SOCKET,
            SO_REUSEADDR,
            &option,
            sizeof(option)
        ) < 0)
    {
        perror("setsockopt UDP");

        close(udp_socket);

        return -1;
    }

    memset(
        &address,
        0,
        sizeof(address)
    );

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(
        (unsigned short)udp_port
    );

    if (bind(
            udp_socket,
            (struct sockaddr *)&address,
            sizeof(address)
        ) < 0)
    {
        perror("bind UDP");

        close(udp_socket);

        return -1;
    }

    return udp_socket;
}


/*
 * Start monitoring.
 *
 * The UDP socket is created BEFORE the MONITOR START command
 * is sent to the Agent. This prevents early UDP packets from
 * being lost.
 */

static int monitor_start(
    int tcp_socket,
    int udp_port
)
{
    char command[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    int udp_socket = create_monitor_socket(
        udp_port
    );

    if (udp_socket < 0)
    {
        return -1;
    }

    snprintf(
        command,
        sizeof(command),
        "MONITOR START %d\n",
        udp_port
    );

    if (send_all(
            tcp_socket,
            command,
            strlen(command)
        ) < 0)
    {
        close(udp_socket);
        return -1;
    }

    int result = receive_line(
        tcp_socket,
        response,
        sizeof(response)
    );

    if (result <= 0)
    {
        close(udp_socket);
        return -1;
    }

    printf(
        "%s",
        response
    );

    if (strncmp(
            response,
            "OK MONITOR_STARTED",
            18
        ) != 0)
    {
        close(udp_socket);
        return 0;
    }

    monitor_context_t context;

    context.socket_fd = udp_socket;
    context.running = 1;

    pthread_t monitor_thread;

    if (pthread_create(
            &monitor_thread,
            NULL,
            monitor_receiver,
            &context
        ) != 0)
    {
        perror("pthread_create");

        close(udp_socket);

        return -1;
    }

    printf(
        "UDP monitoring active on port %d.\n",
        udp_port
    );

    printf(
        "Use MONITOR STOP to stop monitoring.\n"
    );

    /*
     * Wait for the user to enter MONITOR STOP.
     *
     * UDP packets are handled by the monitor thread while
     * this thread waits for keyboard input.
     */

    while (1)
    {
        char input[BUFFER_SIZE];

        printf(
            "RemoteOps> "
        );

        fflush(stdout);

        if (fgets(
                input,
                sizeof(input),
                stdin
            ) == NULL)
        {
            /*
             * Keyboard input closed.
             * Stop local UDP monitoring.
             */

            context.running = 0;

            pthread_join(
                monitor_thread,
                NULL
            );

            close(udp_socket);

            return -1;
        }

        input[strcspn(
            input,
            "\r\n"
        )] = '\0';

        if (strcmp(
                input,
                "MONITOR STOP"
            ) == 0)
        {
            /*
             * Tell the Agent to stop sending UDP packets.
             */

            snprintf(
                command,
                sizeof(command),
                "MONITOR STOP\n"
            );

            if (send_all(
                    tcp_socket,
                    command,
                    strlen(command)
                ) < 0)
            {
                context.running = 0;

                pthread_join(
                    monitor_thread,
                    NULL
                );

                close(udp_socket);

                return -1;
            }

            result = receive_line(
                tcp_socket,
                response,
                sizeof(response)
            );

            if (result > 0)
            {
                printf(
                    "%s",
                    response
                );
            }

            /*
             * Stop local UDP receiver thread.
             */

            context.running = 0;

            pthread_join(
                monitor_thread,
                NULL
            );

            close(udp_socket);

            return 0;
        }

        printf(
            "Monitoring active. Use: MONITOR STOP\n"
        );
    }
}


/*
 * ---------------------------------------------------------
 * Main
 * ---------------------------------------------------------
 */

int main(
    int argc,
    char *argv[]
)
{
    const char *agent_ip = AGENT_IP;
    int agent_port = AGENT_PORT;

    /*
     * Optional:
     *
     * ./controller
     * ./controller 127.0.0.1 9410
     */

    if (argc >= 2)
    {
        agent_ip = argv[1];
    }

    if (argc >= 3)
    {
        agent_port = atoi(argv[2]);

        if (agent_port <= 0 ||
            agent_port > 65535)
        {
            fprintf(
                stderr,
                "Invalid port number.\n"
            );

            return 1;
        }
    }

    if (argc > 3)
    {
        fprintf(
            stderr,
            "Usage: %s [agent_ip] [agent_port]\n",
            argv[0]
        );

        return 1;
    }

    /*
     * Create TCP socket.
     */

    int tcp_socket = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (tcp_socket < 0)
    {
        perror("socket");
        return 1;
    }

    /*
     * Configure Agent address.
     */

    struct sockaddr_in agent_address;

    memset(
        &agent_address,
        0,
        sizeof(agent_address)
    );

    agent_address.sin_family =
        AF_INET;

    agent_address.sin_port =
        htons(
            (unsigned short)agent_port
        );

    if (inet_pton(
            AF_INET,
            agent_ip,
            &agent_address.sin_addr
        ) <= 0)
    {
        fprintf(
            stderr,
            "Invalid Agent IP address: %s\n",
            agent_ip
        );

        close(tcp_socket);

        return 1;
    }

    /*
     * Connect to Agent.
     */

    if (connect(
            tcp_socket,
            (struct sockaddr *)&agent_address,
            sizeof(agent_address)
        ) < 0)
    {
        perror("connect");

        close(tcp_socket);

        return 1;
    }

    printf(
        "Connected to RemoteOps Agent.\n"
    );

    /*
     * AUTH must be the first command.
     */

    char auth_command[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    snprintf(
        auth_command,
        sizeof(auth_command),
        "AUTH %s\n",
        AUTH_TOKEN
    );

    if (send_all(
            tcp_socket,
            auth_command,
            strlen(auth_command)
        ) < 0)
    {
        close(tcp_socket);
        return 1;
    }

    int result = receive_line(
        tcp_socket,
        response,
        sizeof(response)
    );

    if (result <= 0)
    {
        close(tcp_socket);
        return 1;
    }

    printf(
        "Agent response: %s",
        response
    );

    if (strncmp(
            response,
            "OK AUTHENTICATED",
            16
        ) != 0)
    {
        fprintf(
            stderr,
            "Authentication failed.\n"
        );

        close(tcp_socket);

        return 1;
    }

    /*
     * Main command loop.
     */

    char input[BUFFER_SIZE];

    while (1)
    {
        printf(
            "\nRemoteOps> "
        );

        fflush(stdout);

        if (fgets(
                input,
                sizeof(input),
                stdin
            ) == NULL)
        {
            break;
        }

        input[strcspn(
            input,
            "\r\n"
        )] = '\0';

        if (input[0] == '\0')
        {
            continue;
        }

        /*
         * -------------------------------------------------
         * QUIT
         * -------------------------------------------------
         */

        if (strcmp(
                input,
                "QUIT"
            ) == 0)
        {
            char command[BUFFER_SIZE];

            snprintf(
                command,
                sizeof(command),
                "QUIT\n"
            );

            if (send_all(
                    tcp_socket,
                    command,
                    strlen(command)
                ) < 0)
            {
                break;
            }

            result = receive_line(
                tcp_socket,
                response,
                sizeof(response)
            );

            if (result > 0)
            {
                printf(
                    "%s",
                    response
                );
            }

            break;
        }

        /*
         * -------------------------------------------------
         * PUT
         * -------------------------------------------------
         */

        if (strncmp(
                input,
                "PUT ",
                4
            ) == 0)
        {
            char filename[FILENAME_SIZE];

            if (sscanf(
                    input + 4,
                    "%255s",
                    filename
                ) != 1)
            {
                printf(
                    "Usage: PUT <local_filename>\n"
                );

                continue;
            }

            put_file(
                tcp_socket,
                filename
            );

            continue;
        }

        /*
         * -------------------------------------------------
         * GET
         * -------------------------------------------------
         */

        if (strncmp(
                input,
                "GET ",
                4
            ) == 0)
        {
            char filename[FILENAME_SIZE];

            if (sscanf(
                    input + 4,
                    "%255s",
                    filename
                ) != 1)
            {
                printf(
                    "Usage: GET <filename>\n"
                );

                continue;
            }

            get_file(
                tcp_socket,
                filename
            );

            continue;
        }

        /*
         * -------------------------------------------------
         * MONITOR START
         * -------------------------------------------------
         */

        if (strncmp(
                input,
                "MONITOR START ",
                14
            ) == 0)
        {
            int udp_port;

            if (sscanf(
                    input + 14,
                    "%d",
                    &udp_port
                ) != 1)
            {
                printf(
                    "Usage: MONITOR START <udp_port>\n"
                );

                continue;
            }

            if (udp_port <= 0 ||
                udp_port > 65535)
            {
                printf(
                    "Invalid UDP port.\n"
                );

                continue;
            }

            monitor_start(
                tcp_socket,
                udp_port
            );

            continue;
        }

        /*
         * -------------------------------------------------
         * Normal command
         * -------------------------------------------------
         */

        /*
         * Send the existing input directly followed by '\n'.
         * This avoids unnecessary snprintf truncation warnings.
         */

        size_t input_length = strlen(input);

        if (send_all(
                tcp_socket,
                input,
                input_length
            ) < 0)
        {
            break;
        }

        if (send_all(
                tcp_socket,
                "\n",
                1
            ) < 0)
        {
            break;
        }

        result = receive_line(
            tcp_socket,
            response,
            sizeof(response)
        );

        if (result <= 0)
        {
            break;
        }

        printf(
            "%s",
            response
        );
    }

    close(tcp_socket);

    printf(
        "Controller closed.\n"
    );

    return 0;
}
