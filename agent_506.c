#define _POSIX_C_SOURCE 200809L



#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <ctype.h>

#include <dirent.h>

#include <stdint.h>

#include <limits.h>

#include <sys/stat.h>

#include <sys/socket.h>

#include <arpa/inet.h>

#include <pthread.h>
#include <time.h>



#define PORT 9410

#define BUFFER_SIZE 1024



#define AUTH_TOKEN "OPS-3506"

#define SID "6053"



#define LOG_FILE "remoteops_IT24103506.log"

#define STORAGE_PATH "./agentfiles/IT24103506"



#define BACKLOG 5



#define MAX_FILE_SIZE (10ULL * 1024ULL * 1024ULL)



/* Protect concurrent writes to the shared log file. */

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t monitor_condition = PTHREAD_COND_INITIALIZER;



/*

 * Information passed to a Controller connection thread.

 */

typedef struct

{

    int client_socket;

    struct sockaddr_in client_address;

} client_context_t;





/*

 * Per-Controller UDP monitoring state.

 */

typedef struct

{

    pthread_mutex_t mutex;

        pthread_cond_t condition;
pthread_t thread_id;



    int running;

    int stop_requested;



    int udp_socket;



    struct sockaddr_in destination;



} monitor_context_t;





/*

 * Read one complete line from the TCP socket.

 *

 * TCP is a stream, so a command may arrive in multiple

 * recv() calls. This function keeps reading until '\n'.

 */

int read_line(int socket_fd, char *buffer, size_t size)

{

    size_t position = 0;



    while (position < size - 1)

    {

        char character;



        ssize_t received = recv(

            socket_fd,

            &character,

            1,

            0

        );



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

 * Send all bytes in a buffer.

 */

int send_all(

    int socket_fd,

    const void *data,

    size_t length

)

{

    size_t total_sent = 0;



    while (total_sent < length)

    {

        ssize_t sent = send(

            socket_fd,

            (const char *)data + total_sent,

            length - total_sent,

            0

        );



        if (sent <= 0)

        {

            return -1;

        }



        total_sent += (size_t)sent;

    }



    return 0;

}





/*

 * Receive exactly total_bytes from the TCP socket.

 */

ssize_t receive_exact_bytes(

    int client_fd,

    void *buffer,

    size_t total_bytes

)

{

    size_t received = 0;



    while (received < total_bytes)

    {

        ssize_t n = recv(

            client_fd,

            (char *)buffer + received,

            total_bytes - received,

            0

        );



        if (n <= 0)

        {

            return -1;

        }



        received += (size_t)n;

    }



    return (ssize_t)received;

}





/*

 * Get system information.

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





    load_file = fopen(

        "/proc/loadavg",

        "r"

    );



    if (load_file == NULL)

    {

        return -1;

    }



    if (fscanf(

            load_file,

            "%lf",

            &load_1min

        ) != 1)

    {

        fclose(load_file);

        return -1;

    }



    fclose(load_file);





    memory_file = fopen(

        "/proc/meminfo",

        "r"

    );



    if (memory_file == NULL)

    {

        return -1;

    }



    while (fgets(

               line,

               sizeof(line),

               memory_file

           ) != NULL)

    {

        if (sscanf(

                line,

                "MemTotal: %ld kB",

                &mem_total_kb

            ) == 1)

        {

            continue;

        }



        if (sscanf(

                line,

                "MemAvailable: %ld kB",

                &mem_available_kb

            ) == 1)

        {

            continue;

        }

    }



    fclose(memory_file);





    uptime_file = fopen(

        "/proc/uptime",

        "r"

    );



    if (uptime_file == NULL)

    {

        return -1;

    }



    double uptime_value;



    if (fscanf(

            uptime_file,

            "%lf",

            &uptime_value

        ) != 1)

    {

        fclose(uptime_file);

        return -1;

    }



    fclose(uptime_file);





    *cpu_load = load_1min;



    long memory_used_kb =

        mem_total_kb - mem_available_kb;



    *memory_used_mb =

        memory_used_kb / 1024;



    *uptime_sec =

        (long)uptime_value;



    return 0;

}





/*

 * Handle SYSINFO.

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

            "ERR 006 SYSINFO_FAILED SID:%s\n",

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

 * Reads process IDs from /proc and obtains each

 * process name from /proc/<PID>/comm.

 */

void handle_listproc(int client_socket)

{

    DIR *proc_directory;



    struct dirent *entry;



    char response[16384];



    size_t used = 0;



    int first_process = 1;





    proc_directory = opendir(

        "/proc"

    );



    if (proc_directory == NULL)

    {

        char error_response[BUFFER_SIZE];



        snprintf(

            error_response,

            sizeof(error_response),

            "ERR 006 LISTPROC_FAILED SID:%s\n",

            SID

        );



        send_all(

            client_socket,

            error_response,

            strlen(error_response)

        );



        return;

    }





    used = snprintf(

        response,

        sizeof(response),

        "OK PROCS "

    );





    while ((entry = readdir(proc_directory)) != NULL)

    {

        const char *name =

            entry->d_name;





        if (!isdigit(

                (unsigned char)name[0]

            ))

        {

            continue;

        }





        int numeric = 1;



        for (size_t i = 0;

             name[i] != '\0';

             i++)

        {

            if (!isdigit(

                    (unsigned char)name[i]

                ))

            {

                numeric = 0;

                break;

            }

        }





        if (!numeric)

        {

            continue;

        }





        char comm_path[256];



        snprintf(

            comm_path,

            sizeof(comm_path),

            "/proc/%s/comm",

            name

        );





        FILE *comm_file =

            fopen(

                comm_path,

                "r"

            );



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





        process_name[

            strcspn(

                process_name,

                "\r\n"

            )

        ] = '\0';





        char process_entry[200];



        snprintf(

            process_entry,

            sizeof(process_entry),

            "%s/%s",

            name,

            process_name

        );





        size_t required;



        if (first_process)

        {

            required =

                strlen(process_entry);

        }

        else

        {

            required =

                1 + strlen(process_entry);

        }





        size_t sid_space =

            strlen(" SID:6053\n");





        if (used +

                required +

                sid_space >=

            sizeof(response))

        {

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

 * Handle EXEC.

 *

 * Only these five commands are permitted:

 *

 * DATE

 * UPTIME

 * DISKFREE

 * HOSTNAME

 * WHOAMI

 *

 * The user's command is NEVER passed directly to the shell.

 */

void handle_exec(

    int client_socket,

    const char *command_name

)

{

    const char *system_command = NULL;



    char output[BUFFER_SIZE];



    char response[BUFFER_SIZE * 2];





    if (strcmp(

            command_name,

            "DATE"

        ) == 0)

    {

        system_command = "date";

    }

    else if (strcmp(

                 command_name,

                 "UPTIME"

             ) == 0)

    {

        system_command = "uptime";

    }

    else if (strcmp(

                 command_name,

                 "DISKFREE"

             ) == 0)

    {

        system_command =

            "df -h . | tail -n 1";

    }

    else if (strcmp(

                 command_name,

                 "HOSTNAME"

             ) == 0)

    {

        system_command = "hostname";

    }

    else if (strcmp(

                 command_name,

                 "WHOAMI"

             ) == 0)

    {

        system_command = "whoami";

    }

    else

    {

        snprintf(

            response,

            sizeof(response),

            "ERR 002 COMMAND_NOT_ALLOWED SID:%s\n",

            SID

        );



        send_all(

            client_socket,

            response,

            strlen(response)

        );



        return;

    }





    FILE *process =

        popen(

            system_command,

            "r"

        );



    if (process == NULL)

    {

        snprintf(

            response,

            sizeof(response),

            "ERR 006 EXEC_FAILED SID:%s\n",

            SID

        );



        send_all(

            client_socket,

            response,

            strlen(response)

        );



        return;

    }





    size_t used = 0;



    output[0] = '\0';





    while (fgets(

               output + used,

               sizeof(output) - used,

               process

           ) != NULL)

    {

        used = strlen(output);



        if (used >= sizeof(output) - 1)

        {

            break;

        }

    }





    pclose(process);





    output[strcspn(

        output,

        "\r\n"

    )] = '\0';





    snprintf(

        response,

        sizeof(response),

        "OK EXEC_RESULT %s SID:%s\n",

        output,

        SID

    );





    send_all(

        client_socket,

        response,

        strlen(response)

    );

}





/*

 * Write an event to the personalised log file.

 */

void write_log(const char *message)
{
    FILE *log_file;
    time_t current_time;
    struct tm *time_info;
    char timestamp[32];

    pthread_mutex_lock(&log_mutex);

    log_file =
        fopen(
            LOG_FILE,
            "a"
        );

    if (log_file == NULL)
    {
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    current_time = time(NULL);
    time_info = localtime(&current_time);

    if (time_info != NULL)
    {
        strftime(
            timestamp,
            sizeof(timestamp),
            "%Y-%m-%d %H:%M:%S",
            time_info
        );

        fprintf(
            log_file,
            "[%s] %s\n",
            timestamp,
            message
        );
    }
    else
    {
        fprintf(
            log_file,
            "%s\n",
            message
        );
    }

    fclose(log_file);

    pthread_mutex_unlock(&log_mutex);
}

/*

 * Validate a filename.



 *

 * Reject:

 *   /

 *   ..

 *

 * This prevents a client from escaping the personalised

 * storage directory.

 */

int valid_filename(

    const char *filename

)

{

    if (filename == NULL)

    {

        return 0;

    }





    if (filename[0] == '\0')

    {

        return 0;

    }





    if (strstr(

            filename,

            "/"

        ) != NULL)

    {

        return 0;

    }





    if (strstr(

            filename,

            ".."

        ) != NULL)

    {

        return 0;

    }





    return 1;

}





/*

 * Handle PUT.

 *

 * Format:

 *

 * PUT <filename> <filesize>

 *

 * followed immediately by exactly <filesize> raw bytes.

 */

void handle_put(

    int client_socket,

    const char *filename,

    unsigned long long filesize

)

{

    char path[PATH_MAX];



    char response[BUFFER_SIZE];





    if (!valid_filename(filename))

    {

        snprintf(

            response,

            sizeof(response),

            "ERR 006 INVALID_FILENAME SID:%s\n",

            SID

        );



        send_all(

            client_socket,

            response,

            strlen(response)

        );



        return;

    }





    if (filesize > MAX_FILE_SIZE)

    {

        snprintf(

            response,

            sizeof(response),

            "ERR 004 FILE_TOO_LARGE SID:%s\n",

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

        path,

        sizeof(path),

        "%s/%s",

        STORAGE_PATH,

        filename

    );





    FILE *output_file =

        fopen(

            path,

            "wb"

        );



    if (output_file == NULL)

    {

        snprintf(

            response,

            sizeof(response),

            "ERR 006 FILE_WRITE_FAILED SID:%s\n",

            SID

        );



        send_all(

            client_socket,

            response,

            strlen(response)

        );



        return;

    }





    unsigned long long remaining =

        filesize;





    char buffer[4096];





    while (remaining > 0)

    {

        size_t chunk_size;



        if (remaining > sizeof(buffer))

        {

            chunk_size =

                sizeof(buffer);

        }

        else

        {

            chunk_size =

                (size_t)remaining;

        }





        ssize_t received =

            recv(

                client_socket,

                buffer,

                chunk_size,

                0

            );



        if (received <= 0)

        {

            fclose(output_file);



            unlink(path);



            return;

        }





        size_t written =

            fwrite(

                buffer,

                1,

                (size_t)received,

                output_file

            );





        if (written !=

            (size_t)received)

        {

            fclose(output_file);



            unlink(path);



            snprintf(

                response,

                sizeof(response),

                "ERR 006 FILE_WRITE_FAILED SID:%s\n",

                SID

            );



            send_all(

                client_socket,

                response,

                strlen(response)

            );



            return;

        }





        remaining -=

            (unsigned long long)received;

    }





    fclose(output_file);





    snprintf(

        response,

        sizeof(response),

        "OK FILE_RECEIVED %s SID:%s\n",

        filename,

        SID

    );





    send_all(

        client_socket,

        response,

        strlen(response)

    );

}





/*

 * Process a PUT command.

 *

 * Expected:

 *

 * PUT <filename> <filesize>

 */

void process_put_command(

    int client_socket,

    const char *command

)

{

    char filename[256];



    unsigned long long filesize;





    if (sscanf(

            command,

            "PUT %255s %llu",

            filename,

            &filesize

        ) != 2)

    {

        char response[BUFFER_SIZE];



        snprintf(

            response,

            sizeof(response),

            "ERR 006 INVALID_PUT_FORMAT SID:%s\n",

            SID

        );



        send_all(

            client_socket,

            response,

            strlen(response)

        );



        return;

    }





    handle_put(

        client_socket,

        filename,

        filesize

    );

}





/*

 * Handle GET.

 *

 * Format:

 *

 * GET <filename>

 *

 * Response:

 *

 * OK FILE_SEND <filename> <filesize> SID:6053

 *

 * followed immediately by exactly <filesize> raw bytes.

 */

void handle_get(

    int client_socket,

    const char *filename

)

{

    char path[PATH_MAX];



    char response[BUFFER_SIZE];





    /*

     * Validate the filename before constructing the path.

     */

    if (!valid_filename(filename))

    {

        snprintf(

            response,

            sizeof(response),

            "ERR 005 FILE_NOT_FOUND SID:%s\n",

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

     * Build the personalised storage path.

     */

    int path_result = snprintf(

        path,

        sizeof(path),

        "%s/%s",

        STORAGE_PATH,

        filename

    );





    if (path_result < 0 ||

        (size_t)path_result >= sizeof(path))

    {

        snprintf(

            response,

            sizeof(response),

            "ERR 005 FILE_NOT_FOUND SID:%s\n",

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

     * Open the requested file.

     */

    FILE *input_file =

        fopen(

            path,

            "rb"

        );





    if (input_file == NULL)

    {

        snprintf(

            response,

            sizeof(response),

            "ERR 005 FILE_NOT_FOUND SID:%s\n",

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

     * Obtain the file size.

     */

    struct stat file_info;



    if (stat(

            path,

            &file_info

        ) != 0)

    {

        fclose(input_file);



        snprintf(

            response,

            sizeof(response),

            "ERR 005 FILE_NOT_FOUND SID:%s\n",

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

     * Check that the file size is valid.

     */

    if (file_info.st_size < 0)

    {

        fclose(input_file);



        snprintf(

            response,

            sizeof(response),

            "ERR 005 FILE_NOT_FOUND SID:%s\n",

            SID

        );



        send_all(

            client_socket,

            response,

            strlen(response)

        );



        return;

    }





    unsigned long long filesize =

        (unsigned long long)file_info.st_size;





    /*

     * Send the GET header first.

     */

    snprintf(

        response,

        sizeof(response),

        "OK FILE_SEND %s %llu SID:%s\n",

        filename,

        filesize,

        SID

    );





    if (send_all(

            client_socket,

            response,

            strlen(response)

        ) != 0)

    {

        fclose(input_file);

        return;

    }





    /*

     * Send exactly filesize bytes.

     */

    char buffer[4096];



    unsigned long long total_sent =

        0;





    while (total_sent < filesize)

    {

        unsigned long long remaining =

            filesize - total_sent;





        size_t chunk_size;



        if (remaining > sizeof(buffer))

        {

            chunk_size =

                sizeof(buffer);

        }

        else

        {

            chunk_size =

                (size_t)remaining;

        }





        size_t bytes_read =

            fread(

                buffer,

                1,

                chunk_size,

                input_file

            );





        if (bytes_read == 0)

        {

            /*

             * Unexpected end of file or read error.

             */

            fclose(input_file);

            return;

        }





        if (send_all(

                client_socket,

                buffer,

                bytes_read

            ) != 0)

        {

            fclose(input_file);

            return;

        }





        total_sent +=

            (unsigned long long)bytes_read;

    }





    fclose(input_file);

}





/*

 * Process a GET command.

 */

void process_get_command(

    int client_socket,

    const char *command

)

{

    char filename[256];





    if (sscanf(

            command,

            "GET %255s",

            filename

        ) != 1)

    {

        char response[BUFFER_SIZE];



        snprintf(

            response,

            sizeof(response),

            "ERR 005 FILE_NOT_FOUND SID:%s\n",

            SID

        );



        send_all(

            client_socket,

            response,

            strlen(response)

        );



        return;

    }





    handle_get(

        client_socket,

        filename

    );

}





/*

 * UDP monitoring thread.

 *

 * Sends SYSINFO-style datagrams every 2 seconds.

 */

void *monitor_thread(void *argument)
{
    monitor_context_t *monitor =
        (monitor_context_t *)argument;

    while (1)
    {
        double cpu_load;
        long memory_used_mb;
        long uptime_sec;

        /*
         * Collect and send one SYSINFO datagram.
         */
        if (get_system_info(
                &cpu_load,
                &memory_used_mb,
                &uptime_sec
            ) == 0)
        {
            char datagram[BUFFER_SIZE];

            snprintf(
                datagram,
                sizeof(datagram),
                "SYSINFO %.2f %ld %ld SID:%s\n",
                cpu_load,
                memory_used_mb,
                uptime_sec,
                SID
            );

            pthread_mutex_lock(&monitor->mutex);

            int socket_fd = monitor->udp_socket;
            struct sockaddr_in destination =
                monitor->destination;

            pthread_mutex_unlock(&monitor->mutex);

            if (sendto(
                    socket_fd,
                    datagram,
                    strlen(datagram),
                    0,
                    (struct sockaddr *)&destination,
                    sizeof(destination)
                ) < 0)
            {
                /*
                 * The Controller may have disconnected.
                 * The monitoring thread will still terminate
                 * when stop_requested is signalled.
                 */
            }
        }

        /*
         * Wait for 2 seconds, but wake immediately when
         * MONITOR STOP or Controller disconnect occurs.
         */
        struct timespec timeout;

        if (clock_gettime(
                CLOCK_REALTIME,
                &timeout
            ) != 0)
        {
            sleep(2);
            continue;
        }

        timeout.tv_sec += 2;

        pthread_mutex_lock(&monitor->mutex);

        if (!monitor->stop_requested)
        {
            pthread_cond_timedwait(
                &monitor->condition,
                &monitor->mutex,
                &timeout
            );
        }

        int should_stop =
            monitor->stop_requested;

        pthread_mutex_unlock(&monitor->mutex);

        if (should_stop)
        {
            break;
        }
    }

    pthread_mutex_lock(&monitor->mutex);

    int socket_fd =
        monitor->udp_socket;

    monitor->udp_socket = -1;

    pthread_mutex_unlock(&monitor->mutex);

    if (socket_fd >= 0)
    {
        close(socket_fd);
    }

    return NULL;
}


/*
 * Start UDP monitoring for one Controller.
 */

int start_monitor(

    monitor_context_t *monitor,

    const struct sockaddr_in *controller_address,

    unsigned int udp_port

)

{

    pthread_mutex_lock(

        &monitor->mutex

    );





    if (monitor->running)

    {

        pthread_mutex_unlock(

            &monitor->mutex

        );



        return 1;

    }





    monitor->udp_socket =

        socket(

            AF_INET,

            SOCK_DGRAM,

            0

        );





    if (monitor->udp_socket < 0)

    {

        pthread_mutex_unlock(

            &monitor->mutex

        );



        return -1;

    }





    monitor->destination =

        *controller_address;





    monitor->destination.sin_port =

        htons(

            (uint16_t)udp_port

        );





    monitor->stop_requested = 0;



    monitor->running = 1;





    int result =

        pthread_create(

            &monitor->thread_id,

            NULL,

            monitor_thread,

            monitor

        );





    if (result != 0)

    {

        close(

            monitor->udp_socket

        );



        monitor->running = 0;



        pthread_mutex_unlock(

            &monitor->mutex

        );



        return -1;

    }





    pthread_mutex_unlock(

        &monitor->mutex

    );





    return 0;

}





/*

 * Stop UDP monitoring for one Controller.

 */

void stop_monitor(

    monitor_context_t *monitor

)

{

    pthread_mutex_lock(

        &monitor->mutex

    );





    if (!monitor->running)

    {

        pthread_mutex_unlock(

            &monitor->mutex

        );



        return;

    }





    monitor->stop_requested = 1;





    
    /*
     * Wake the monitoring thread immediately so that
     * MONITOR STOP does not wait for the next 2-second interval.
     */
    pthread_cond_signal(
        &monitor->condition
    );

pthread_t thread_id =

        monitor->thread_id;





    pthread_mutex_unlock(

        &monitor->mutex

    );





    pthread_join(

        thread_id,

        NULL

    );





    pthread_mutex_lock(

        &monitor->mutex

    );





    monitor->running = 0;





    pthread_mutex_unlock(

        &monitor->mutex

    );

}





/*

 * Handle one Controller connection.

 *

 * A separate detached thread is created for every accepted

 * Controller socket. This allows multiple Controllers to use

 * the Agent at the same time.

 */

void *client_thread(void *argument)

{

    client_context_t *context =

        (client_context_t *)argument;



    int client_socket =

        context->client_socket;





    struct sockaddr_in client_address =

        context->client_address;





    free(context);





    monitor_context_t monitor;





    pthread_mutex_init(

        &monitor.mutex,

        NULL

    );





    
    pthread_cond_init(
        &monitor.condition,
        NULL
    );

monitor.running = 0;



    monitor.stop_requested = 0;



    monitor.udp_socket = -1;





    printf(

        "Controller connected.\n"

    );





    write_log(

        "Controller connected"

    );





    /*

     * Each connection starts unauthenticated.

     */

    int authenticated = 0;





    while (1)

    {

        char command[BUFFER_SIZE];





        int result =

            read_line(

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

         * Remove CR/LF.

         */

        command[

            strcspn(

                command,

                "\r\n"

            )

        ] = '\0';





        printf(

            "Received: %s\n",

            command

        );





        /*

         * AUTH

         */

        if (strncmp(

                command,

                "AUTH ",

                5

            ) == 0)

        {

            const char *token =

                command + 5;





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

         * Reject commands before successful AUTH.

         */

        if (!authenticated)

        {

            char response[BUFFER_SIZE];





            snprintf(

                response,

                sizeof(response),

                "ERR 003 NOT_AUTHENTICATED SID:%s\n",

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

         * EXEC

         */

        if (strncmp(

                command,

                "EXEC ",

                5

            ) == 0)

        {

            const char *command_name =

                command + 5;





            handle_exec(

                client_socket,

                command_name

            );





            write_log(

                "EXEC command"

            );





            continue;

        }





        /*

         * PUT

         */

        if (strncmp(

                command,

                "PUT ",

                4

            ) == 0)

        {

            process_put_command(

                client_socket,

                command

            );





            write_log(

                "PUT command"

            );





            continue;

        }





        /*

         * GET

         */

        if (strncmp(

                command,

                "GET ",

                4

            ) == 0)

        {

            process_get_command(

                client_socket,

                command

            );





            write_log(

                "GET command"

            );





            continue;

        }





        /*

         * MONITOR START

         */

        if (strncmp(

                command,

                "MONITOR START ",

                14

            ) == 0)

        {

            unsigned int udp_port;



            char extra;





            /*

             * Accept exactly:

             *

             * MONITOR START <udp_port>

             */

            if (sscanf(

                    command + 14,

                    "%u %c",

                    &udp_port,

                    &extra

                ) != 1 ||

                udp_port < 1 ||

                udp_port > 65535)

            {

                char response[BUFFER_SIZE];





                snprintf(

                    response,

                    sizeof(response),

                    "ERR 006 INVALID_UDP_PORT SID:%s\n",

                    SID

                );





                send_all(

                    client_socket,

                    response,

                    strlen(response)

                );





                write_log(

                    "MONITOR START invalid UDP port"

                );





                continue;

            }





            int monitor_result =

                start_monitor(

                    &monitor,

                    &client_address,

                    udp_port

                );





            char response[BUFFER_SIZE];





            if (monitor_result == 0)

            {

                snprintf(

                    response,

                    sizeof(response),

                    "OK MONITOR_STARTED SID:%s\n",

                    SID

                );





                write_log(

                    "MONITOR START"

                );

            }

            else if (monitor_result == 1)

            {

                snprintf(

                    response,

                    sizeof(response),

                    "ERR 006 MONITOR_ALREADY_RUNNING SID:%s\n",

                    SID

                );





                write_log(

                    "MONITOR START while already running"

                );

            }

            else

            {

                snprintf(

                    response,

                    sizeof(response),

                    "ERR 006 MONITOR_START_FAILED SID:%s\n",

                    SID

                );





                write_log(

                    "MONITOR START failed"

                );

            }





            send_all(

                client_socket,

                response,

                strlen(response)

            );





            continue;

        }





        /*

         * MONITOR STOP

         */

        if (strcmp(

                command,

                "MONITOR STOP"

            ) == 0)

        {

            stop_monitor(

                &monitor

            );





            char response[BUFFER_SIZE];





            snprintf(

                response,

                sizeof(response),

                "OK MONITOR_STOPPED SID:%s\n",

                SID

            );





            send_all(

                client_socket,

                response,

                strlen(response)

            );





            write_log(

                "MONITOR STOP"

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





            stop_monitor(

                &monitor

            );





            break;

        }





        /*

         * Unknown command.

         */

        {

            char response[BUFFER_SIZE];





            snprintf(

                response,

                sizeof(response),

                "ERR 006 UNKNOWN_COMMAND SID:%s\n",

                SID

            );





            send_all(

                client_socket,

                response,

                strlen(response)

            );





            write_log(

                "Unknown command"

            );

        }

    }





    /*

     * Always stop monitoring before the

     * Controller connection is destroyed.

     */

    stop_monitor(

        &monitor

    );





    pthread_cond_destroy(
        &monitor.condition
    );

    pthread_mutex_destroy(

        &monitor.mutex

    );





    close(

        client_socket

    );





    return NULL;

}





/*

 * Main Agent.

 */

int main(void)

{

    int server_socket;





    struct sockaddr_in server_address;

    struct sockaddr_in client_address;





    socklen_t client_length;





    /*

     * Create storage directories.

     */

    mkdir(

        "./agentfiles",

        0755

    );





    mkdir(

        STORAGE_PATH,

        0755

    );





    /*

     * Create TCP socket.

     */

    server_socket =

        socket(

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

     * Allow quick restart.

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





    server_address.sin_family =

        AF_INET;





    server_address.sin_addr.s_addr =

        htonl(INADDR_ANY);





    server_address.sin_port =

        htons(PORT);





    /*

     * Bind to personalised port.

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

     * Listen for Controller connections.

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

     * Each accepted Controller is handed to a separate

     * detached pthread so that multiple Controllers can

     * be served concurrently.

     */

    while (1)

    {

        client_length =

            sizeof(client_address);





        int client_socket =

            accept(

                server_socket,

                (struct sockaddr *)&client_address,

                &client_length

            );





        if (client_socket < 0)

        {

            perror("accept");

            continue;

        }





        client_context_t *context =

            malloc(

                sizeof(client_context_t)

            );





        if (context == NULL)

        {

            perror("malloc");





            close(client_socket);





            continue;

        }





        context->client_socket =

            client_socket;





        context->client_address =

            client_address;





        pthread_t thread_id;





        int thread_result =

            pthread_create(

                &thread_id,

                NULL,

                client_thread,

                context

            );





        if (thread_result != 0)

        {

            fprintf(

                stderr,

                "pthread_create failed: %s\n",

                strerror(thread_result)

            );





            close(client_socket);





            free(context);





            continue;

        }





        pthread_detach(

            thread_id

        );

    }





    close(

        server_socket

    );





    return 0;

}
