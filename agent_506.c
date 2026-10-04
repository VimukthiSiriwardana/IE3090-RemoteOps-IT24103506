#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 9410
#define BUFFER_SIZE 1024

#define AUTH_TOKEN "OPS-3506"
#define SID "6053"

#define LOG_FILE "remoteops_IT24103506.log"
#define STORAGE_PATH "./agentfiles/IT24103506"

#define BACKLOG 5


/*
 * Read one complete line from the TCP socket.
 *
 * TCP is a stream, so one recv() call does not necessarily
 * contain one complete command.
 */
int read_line(int socket_fd, char *buffer, size_t size)
{
    size_t position = 0;

    while (position < size - 1)
    {
        char character;
        ssize_t received = recv(socket_fd, &character, 1, 0);

        if (received == 0)
        {
            return 0;
        }

        if (received < 0)
        {
            return -1;
        }

        buffer[position++] = character;

        if (character == '\n')
        {
            break;
        }
    }

    buffer[position] = '\0';

    return 1;
}


/*
 * Send the complete response to the TCP client.
 *
 * send() is not guaranteed to send all bytes at once,
 * therefore we keep sending until the whole response is sent.
 */
int send_all(int socket_fd, const char *data, size_t length)
{
    size_t total_sent = 0;

    while (total_sent < length)
    {
        ssize_t sent = send(
            socket_fd,
            data + total_sent,
            length - total_sent,
            0
        );

        if (sent <= 0)
        {
            return -1;
        }

        total_sent += sent;
    }

    return 0;
}


/*
 * Get basic system information from Linux /proc files.
 *
 * CPU load     -> /proc/loadavg
 * Memory usage -> /proc/meminfo
 * Uptime       -> /proc/uptime
 */
int get_system_info(
    double *cpu_load,
    long *memory_used_mb,
    long *uptime_sec
)
{
    FILE *load_file;
    FILE *memory_file;
    FILE *uptime_file;

    double load_1min;
    long mem_total_kb = 0;
    long mem_available_kb = 0;
    char line[256];

    load_file = fopen("/proc/loadavg", "r");

    if (load_file == NULL)
    {
        return -1;
    }

    if (fscanf(load_file, "%lf", &load_1min) != 1)
    {
        fclose(load_file);
        return -1;
    }

    fclose(load_file);


    memory_file = fopen("/proc/meminfo", "r");

    if (memory_file == NULL)
    {
        return -1;
    }

    while (fgets(line, sizeof(line), memory_file) != NULL)
    {
        if (sscanf(line, "MemTotal: %ld kB", &mem_total_kb) == 1)
        {
            continue;
        }

        if (sscanf(line, "MemAvailable: %ld kB", &mem_available_kb) == 1)
        {
            continue;
        }
    }

    fclose(memory_file);


    uptime_file = fopen("/proc/uptime", "r");

    if (uptime_file == NULL)
    {
        return -1;
    }

    double uptime_value;

    if (fscanf(uptime_file, "%lf", &uptime_value) != 1)
    {
        fclose(uptime_file);
        return -1;
    }

    fclose(uptime_file);


    *cpu_load = load_1min;

    /*
     * Convert KB to MB.
     */
    long memory_used_kb = mem_total_kb - mem_available_kb;

    *memory_used_mb = memory_used_kb / 1024;

    *uptime_sec = (long)uptime_value;

    return 0;
}


/*
 * Handle the SYSINFO command.
 *
 * Required protocol:
 *
 * OK SYSINFO <cpu_load> <mem_used_mb> <uptime_sec> SID:<sid>
 */
void handle_sysinfo(int client_socket)
{
    double cpu_load;
    long memory_used_mb;
    long uptime_sec;

    char response[BUFFER_SIZE];

    if (get_system_info(
            &cpu_load,
            &memory_used_mb,
            &uptime_sec
        ) != 0)
    {
        snprintf(
            response,
            sizeof(response),
            "ERR 003 SYSINFO_FAILED SID:%s\n",
            SID
        );

        send_all(
            client_socket,
            response,
            strlen(response)
        );

        return;
    }


    snprintf(
        response,
        sizeof(response),
        "OK SYSINFO %.2f %ld %ld SID:%s\n",
        cpu_load,
        memory_used_mb,
        uptime_sec,
        SID
    );

    send_all(
        client_socket,
        response,
        strlen(response)
    );
}


/*
 * Handle LISTPROC.
 *
 * The assignment requires:
 *
 * OK PROCS <comma-separated process names/PIDs> SID:<sid>
 *
 * Linux exposes running processes through numeric
 * directories inside /proc.
 *
 * Example:
 *
 * /proc/1
 * /proc/100
 * /proc/101
 */
void handle_listproc(int client_socket)
{
    DIR *proc_directory;
    struct dirent *entry;

    char response[BUFFER_SIZE];

    size_t used = 0;
    int first_process = 1;


    proc_directory = opendir("/proc");

    if (proc_directory == NULL)
    {
        snprintf(
            response,
            sizeof(response),
            "ERR 003 LISTPROC_FAILED SID:%s\n",
            SID
        );

        send_all(
            client_socket,
            response,
            strlen(response)
        );

        return;
    }


    /*
     * Start the response.
     */
    used = snprintf(
        response,
        sizeof(response),
        "OK PROCS "
    );


    while ((entry = readdir(proc_directory)) != NULL)
    {
        const char *name = entry->d_name;

        /*
         * Process directories have numeric names.
         *
         * Ignore:
         * .
         * ..
         * other /proc entries
         */
        if (!isdigit((unsigned char)name[0]))
        {
            continue;
        }


        /*
         * Make sure the complete directory name
         * contains only digits.
         */
        int numeric = 1;

        for (size_t i = 0; name[i] != '\0'; i++)
        {
            if (!isdigit((unsigned char)name[i]))
            {
                numeric = 0;
                break;
            }
        }

        if (!numeric)
        {
            continue;
        }


        /*
         * Read the process name from:
         *
         * /proc/<PID>/comm
         */
        char comm_path[256];

        snprintf(
            comm_path,
            sizeof(comm_path),
            "/proc/%s/comm",
            name
        );


        FILE *comm_file = fopen(comm_path, "r");

        if (comm_file == NULL)
        {
            continue;
        }


        char process_name[128];

        if (fgets(
                process_name,
                sizeof(process_name),
                comm_file
            ) == NULL)
        {
            fclose(comm_file);
            continue;
        }

        fclose(comm_file);


        /*
         * Remove newline from process name.
         */
        process_name[strcspn(process_name, "\r\n")] = '\0';


        /*
         * Calculate how much space this process
         * will need in the response.
         */
        char process_entry[200];

        snprintf(
            process_entry,
            sizeof(process_entry),
            "%s/%s",
            name,
            process_name
        );


        /*
         * Add comma between processes.
         */
        size_t required;

        if (first_process)
        {
            required = strlen(process_entry);
        }
        else
        {
            required = 1 + strlen(process_entry);
        }


        /*
         * Leave enough room for:
         *
         *  SID:6053\n
         */
        size_t sid_space = strlen(" SID:6053\n");


        if (used + required + sid_space >= sizeof(response))
        {
            /*
             * Response buffer is full.
             *
             * We stop adding more processes rather than
             * overflowing the buffer.
             */
            break;
        }


        if (!first_process)
        {
            response[used++] = ',';
        }


        memcpy(
            response + used,
            process_entry,
            strlen(process_entry)
        );

        used += strlen(process_entry);

        response[used] = '\0';

        first_process = 0;
    }


    closedir(proc_directory);


    /*
     * Add the required SID tag.
     */
    snprintf(
        response + used,
        sizeof(response) - used,
        " SID:%s\n",
        SID
    );


    send_all(
        client_socket,
        response,
        strlen(response)
    );
}


/*
 * Write an activity message to the personalised log file.
 *
 * Logging will be expanded later to cover all commands
 * and file transfers.
 */
void write_log(const char *message)
{
    FILE *log_file = fopen(LOG_FILE, "a");

    if (log_file == NULL)
    {
        return;
    }

    fprintf(
        log_file,
        "%s\n",
        message
    );

    fclose(log_file);
}


int main(void)
{
    int server_socket;
    int client_socket;

    struct sockaddr_in server_address;
    struct sockaddr_in client_address;

    socklen_t client_length;


    /*
     * Create TCP socket.
     */
    server_socket = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (server_socket < 0)
    {
        perror("socket");
        return 1;
    }


    /*
     * Allow quick restart of the server.
     */
    int option = 1;

    if (setsockopt(
            server_socket,
            SOL_SOCKET,
            SO_REUSEADDR,
            &option,
            sizeof(option)
        ) < 0)
    {
        perror("setsockopt");

        close(server_socket);

        return 1;
    }


    /*
     * Configure server address.
     */
    memset(
        &server_address,
        0,
        sizeof(server_address)
    );

    server_address.sin_family = AF_INET;

    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    server_address.sin_port = htons(PORT);


    /*
     * Bind socket to personalised port 9410.
     */
    if (bind(
            server_socket,
            (struct sockaddr *)&server_address,
            sizeof(server_address)
        ) < 0)
    {
        perror("bind");

        close(server_socket);

        return 1;
    }


    /*
     * Start listening for Controller connections.
     */
    if (listen(
            server_socket,
            BACKLOG
        ) < 0)
    {
        perror("listen");

        close(server_socket);

        return 1;
    }


    printf(
        "RemoteOps Agent started.\n"
    );

    printf(
        "Listening on TCP port %d\n",
        PORT
    );

    printf(
        "Session ID: %s\n",
        SID
    );

    printf(
        "Authentication token: %s\n",
        AUTH_TOKEN
    );


    write_log(
        "Agent started"
    );


    /*
     * Main connection loop.
     *
     * At this stage the Agent is single-threaded.
     * We will add pthread-based concurrency later.
     */
    while (1)
    {
        client_length = sizeof(client_address);


        client_socket = accept(
            server_socket,
            (struct sockaddr *)&client_address,
            &client_length
        );


        if (client_socket < 0)
        {
            perror("accept");

            continue;
        }


        printf(
            "Controller connected.\n"
        );


        write_log(
            "Controller connected"
        );


        /*
         * Each TCP connection starts unauthenticated.
         */
        int authenticated = 0;


        while (1)
        {
            char command[BUFFER_SIZE];


            int result = read_line(
                client_socket,
                command,
                sizeof(command)
            );


            /*
             * Client disconnected.
             */
            if (result == 0)
            {
                printf(
                    "Controller disconnected.\n"
                );

                write_log(
                    "Controller disconnected"
                );

                break;
            }


            /*
             * recv() error.
             */
            if (result < 0)
            {
                perror("recv");

                write_log(
                    "Controller connection error"
                );

                break;
            }


            /*
             * Remove newline characters.
             */
            command[strcspn(
                command,
                "\r\n"
            )] = '\0';


            printf(
                "Received: %s\n",
                command
            );


            /*
             * AUTH must be the first command.
             */
            if (strncmp(
                    command,
                    "AUTH ",
                    5
                ) == 0)
            {
                const char *token = command + 5;


                if (strcmp(
                        token,
                        AUTH_TOKEN
                    ) == 0)
                {
                    authenticated = 1;


                    char response[BUFFER_SIZE];

                    snprintf(
                        response,
                        sizeof(response),
                        "OK AUTHENTICATED SID:%s\n",
                        SID
                    );


                    send_all(
                        client_socket,
                        response,
                        strlen(response)
                    );


                    write_log(
                        "AUTH successful"
                    );
                }
                else
                {
                    authenticated = 0;


                    char response[BUFFER_SIZE];

                    snprintf(
                        response,
                        sizeof(response),
                        "ERR 001 AUTH_FAILED SID:%s\n",
                        SID
                    );


                    send_all(
                        client_socket,
                        response,
                        strlen(response)
                    );


                    write_log(
                        "AUTH failed"
                    );
                }


                continue;
            }


            /*
             * Reject every command until authentication
             * has succeeded.
             */
            if (!authenticated)
            {
                char response[BUFFER_SIZE];

                snprintf(
                    response,
                    sizeof(response),
                    "ERR 001 AUTH_FAILED SID:%s\n",
                    SID
                );


                send_all(
                    client_socket,
                    response,
                    strlen(response)
                );


                write_log(
                    "Rejected unauthenticated command"
                );


                continue;
            }


            /*
             * SYSINFO
             */
            if (strcmp(
                    command,
                    "SYSINFO"
                ) == 0)
            {
                handle_sysinfo(
                    client_socket
                );


                write_log(
                    "SYSINFO command"
                );


                continue;
            }


            /*
             * LISTPROC
             */
            if (strcmp(
                    command,
                    "LISTPROC"
                ) == 0)
            {
                handle_listproc(
                    client_socket
                );


                write_log(
                    "LISTPROC command"
                );


                continue;
            }


            /*
             * QUIT
             */
            if (strcmp(
                    command,
                    "QUIT"
                ) == 0)
            {
                char response[BUFFER_SIZE];


                snprintf(
                    response,
                    sizeof(response),
                    "OK BYE SID:%s\n",
                    SID
                );


                send_all(
                    client_socket,
                    response,
                    strlen(response)
                );


                write_log(
                    "QUIT command"
                );


                break;
            }


            /*
             * Commands not implemented yet.
             *
             * We will replace this section later with:
             *
             * EXEC
             * PUT
             * GET
             * MONITOR START
             * MONITOR STOP
             */
            {
                char response[BUFFER_SIZE];


                snprintf(
                    response,
                    sizeof(response),
                    "ERR 003 COMMAND_NOT_IMPLEMENTED SID:%s\n",
                    SID
                );


                send_all(
                    client_socket,
                    response,
                    strlen(response)
                );
            }
        }


        /*
         * Close this Controller connection.
         */
        close(client_socket);


        printf(
            "Connection closed.\n"
        );
    }


    close(server_socket);

    return 0;
}
