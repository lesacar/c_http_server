#include <microhttpd.h>
#include <assert.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

#define PORT 8080

struct ConnectionData {
    char *index_page_text;
    size_t index_page_size;
};

struct ClientSet {
    char **addresses;
    size_t count;
    size_t capacity;
};

struct CombinedData {
    struct ClientSet *clientSet;
    struct ConnectionData *connectionData;
};

// Function to check if an address is already in the set
int isAddressInSet(struct ClientSet *clientSet, const char *address) {
    if (clientSet == NULL || clientSet->addresses == NULL) {
        //fprintf(stderr, "isAddressInSet: Invalid clientSet (NULL)\n");
        return 0;  // Address is not in the set
    }

    for (size_t i = 0; i < clientSet->count; ++i) {
        // Case-insensitive comparison
        if (strcasecmp(clientSet->addresses[i], address) == 0) {
            //fprintf(stderr, "isAddressInSet: Address '%s' already in the set\n", address);
            return 1;  // Address is in the set
        }
    }

    return 0;  // Address is not in the set
}

void addToSet(struct ClientSet *clientSet, const char *address) {
    // Check for NULL pointer
    if (clientSet == NULL) {
        fprintf(stderr, "addToSet: clientSet is NULL\n");
        return;
    }

    // Check for valid address
    if (address == NULL || address[0] == '\0') {
        fprintf(stderr, "addToSet: Invalid address\n");
        return;
    }

    // Check if the address is already in the set
    if (isAddressInSet(clientSet, address)) {
        printf("addToSet: Duplicate entry\n");
        return;
    }

    // Check if the set needs to be resized
    if (clientSet->count == clientSet->capacity) {
        size_t newCapacity = clientSet->capacity * 2 + 1;  // increase the capacity
        clientSet->addresses = realloc(clientSet->addresses, newCapacity * sizeof(char *));
        if (clientSet->addresses == NULL) {
            perror("addToSet: realloc failed");
            exit(EXIT_FAILURE);  // or handle the error appropriately
        }
        clientSet->capacity = newCapacity;
    }

    // Allocate memory for the new address and copy it
    clientSet->addresses[clientSet->count] = strdup(address);
    if (clientSet->addresses[clientSet->count] == NULL) {
        perror("addToSet: strdup failed");
        exit(EXIT_FAILURE);  // or handle the error appropriately
    }

    // Increment the count
    ++clientSet->count;
}
enum MHD_Result answer_to_connection(void *cls, struct MHD_Connection *connection,
                                     const char *url, const char *method,
                                     const char *version, const char *upload_data,
                                     size_t *upload_data_size, void **con_cls);

void request_completed(void *cls, struct MHD_Connection *connection,
                       void **con_cls, enum MHD_RequestTerminationCode toe);

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <path_to_page_directory>\n", argv[0]);
        return 1;
    }

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd:");
        return 1;
    }

    chdir(argv[1]);

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("cwd: %s\n", cwd);
    } else {
        perror("getcwd:");
        return 1;
    }

    FILE *index_page = fopen("index.html", "rb");
    if (index_page == NULL) {
        perror("fopen:");
        return 1;
    }


    FILE *connections_log = fopen("connections.log", "a");
    if (connections_log == NULL) {
        perror("fopen:");
        return 1;
    }

    time_t init_time = time(NULL);
    struct tm *tm = localtime(&init_time);
    char init_time_str[64];
    size_t ret = strftime(init_time_str, sizeof(init_time_str), "%c", tm);
    assert(ret);
    fprintf(connections_log, "Server started - %s\n", init_time_str);
    fflush(connections_log);

    fseek(index_page, 0L, SEEK_END);
    size_t index_page_size = ftell(index_page);
    rewind(index_page);

    char *index_page_text = (char *)malloc(index_page_size + 1);
    if (index_page_text == NULL) {
        perror("Error allocating memory");
        fclose(index_page);
        return 1;
    }

    fread(index_page_text, 1, index_page_size, index_page);
    index_page_text[index_page_size] = '\0';

    struct CombinedData *combinedData = malloc(sizeof(struct CombinedData));
    if (combinedData == NULL) {
        perror("Error allocating memory");
        free(index_page_text);
        //free(connection_data);
        fclose(index_page);
        return 1;
    }

        //struct ConnectionData *connection_data = malloc(sizeof(struct ConnectionData));

        combinedData->connectionData = malloc(sizeof(struct ConnectionData));
            if (combinedData->connectionData == NULL) {
        perror("Error allocating memory");
        free(index_page_text);
        fclose(index_page);
        return 1;
    }

    combinedData->connectionData->index_page_text = strdup(index_page_text);
    if (combinedData->connectionData->index_page_text == NULL) {
    perror("Error allocating memory for index_page_text");
    free(index_page_text);
    fclose(index_page);
    free(combinedData->clientSet);
    free(combinedData->connectionData);
    free(combinedData);
    return 1;
}
    combinedData->clientSet = malloc(sizeof(struct ClientSet));
    if (combinedData->clientSet == NULL) {
        perror("Error allocating memory");
        free(index_page_text);
        free(combinedData->connectionData);
        fclose(index_page);
        free(combinedData);
        return 1;
    }

    combinedData->clientSet->addresses = NULL;
    combinedData->clientSet->count = 0;
    combinedData->clientSet->capacity = 0;
    //combinedData->connectionData = connection_data;
    combinedData->connectionData->index_page_text = index_page_text;
    combinedData->connectionData->index_page_size = index_page_size;

    struct MHD_Daemon *daemon;

    daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL,
                              &answer_to_connection, combinedData,
                              NULL, &request_completed,
                              MHD_OPTION_END);

    if (daemon == NULL) {
        fprintf(stderr, "Error starting daemon: %s\n", strerror(errno));
        free(index_page_text);
        free(combinedData->connectionData);
        fclose(index_page);
        free(combinedData->clientSet->addresses);
        free(combinedData->clientSet);
        free(combinedData);
        return 1;
    }

    printf("Server initialized! Press ENTER to exit\nMonitoring...\n");
    getchar();
    fflush(stdin);
    chdir(cwd);

    MHD_stop_daemon(daemon);

    free(index_page_text);
    free(combinedData->connectionData);
    fclose(index_page);

    for (size_t i = 0; i < combinedData->clientSet->count; ++i) {
        free(combinedData->clientSet->addresses[i]);
    }
    free(combinedData->clientSet->addresses);
    free(combinedData->clientSet);
    free(combinedData);

    time_t end_time = time(NULL);
    struct tm *tm_end = localtime(&end_time);
    char end_time_str[64];
    size_t ret_end = strftime(end_time_str, sizeof(end_time_str), "%c", tm_end);
    assert(ret_end);
    fprintf(connections_log, "Server closed - %s\n", end_time_str);
    fflush(connections_log);
    free(connections_log);

    printf("Server shutdown complete, exiting...\n");
    return 0;
}

enum MHD_Result answer_to_connection(void *cls, struct MHD_Connection *connection,
                                     const char *url, const char *method,
                                     const char *version, const char *upload_data,
                                     size_t *upload_data_size, void **con_cls) {
    struct CombinedData *combinedData = (struct CombinedData *)cls;

    if (combinedData == NULL) {
        fprintf(stderr, "answer_to_connection: Invalid CombinedData (NULL)\n");
        fflush(stderr);
        return MHD_NO;
    }

    struct ClientSet *clientSet = combinedData->clientSet;
    struct ConnectionData *connectionData = combinedData->connectionData;

    char full_path[PATH_MAX];
    struct sockaddr *client_addr;
    socklen_t addr_len;
    struct sockaddr_in6 default_addr6;

    const union MHD_ConnectionInfo *info = MHD_get_connection_info(connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS, &default_addr6, sizeof(default_addr6), &addr_len);
    client_addr = (struct sockaddr *)(info->client_addr);

    char client_address[INET6_ADDRSTRLEN];
    if (client_addr->sa_family == AF_INET) {
        inet_ntop(AF_INET, &(((struct sockaddr_in *)client_addr)->sin_addr), client_address, INET_ADDRSTRLEN);
    } else if (client_addr->sa_family == AF_INET6) {
        inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)client_addr)->sin6_addr), client_address, INET6_ADDRSTRLEN);
    } else {
        strcpy(client_address, "127.0.0.1");
    }

    // Check if clientSet is NULL, and initialize it if needed
    if (clientSet == NULL) {
        clientSet = malloc(sizeof(struct ClientSet));
        if (clientSet == NULL) {
            perror("Error creating client set");
            exit(EXIT_FAILURE);
        }
        clientSet->addresses = NULL;
        clientSet->count = 0;
        clientSet->capacity = 0;
        combinedData->clientSet = clientSet;  // Update combinedData with the new clientSet
        *con_cls = clientSet;  // Set the clientSet for this connection
    }

    // Check if the address is not in the set, and add it
    if (!isAddressInSet(clientSet, client_address)) {
        addToSet(clientSet, client_address);

        if (url != NULL && strcmp(url, "/") == 0) {
            FILE *log_file = fopen("connections.log", "a");
            if (log_file != NULL) {
                fprintf(log_file, "Unique Connection: %s\n", client_address);
                fclose(log_file);
            }
        }
    }


    if (strcmp(url, "/") == 0) {
        snprintf(full_path, sizeof(full_path), "./index.html");

        FILE *file = fopen(full_path, "rb");
        if (file == NULL) {
            struct MHD_Response *response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
            int ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
            MHD_destroy_response(response);
            return ret;
        }

        struct stat file_stat;
        if (fstat(fileno(file), &file_stat) != 0) {
            struct MHD_Response *response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
            int ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
            MHD_destroy_response(response);
            fclose(file);
            return ret;
        }

        char *file_content = (char *)malloc(file_stat.st_size);
        if (file_content == NULL) {
            perror("Error allocating memory");
            fclose(file);

            struct MHD_Response *response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
            int ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
            MHD_destroy_response(response);
            return ret;
        }

        fread(file_content, 1, file_stat.st_size, file);
        fclose(file);

        struct MHD_Response *response = MHD_create_response_from_buffer(file_stat.st_size, file_content, MHD_RESPMEM_MUST_COPY);
        MHD_add_response_header(response, "Content-Type", "text/html");
        int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        free(file_content);
        return ret;
    } else {
        snprintf(full_path, sizeof(full_path), ".%s", url);

        FILE *file = fopen(full_path, "rb");
        if (file == NULL) {
            struct MHD_Response *response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
            int ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
            MHD_destroy_response(response);
            return ret;
        }

        struct stat file_stat;
        if (fstat(fileno(file), &file_stat) != 0) {
            struct MHD_Response *response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
            int ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
            MHD_destroy_response(response);
            fclose(file);
            return ret;
        }

        char *file_content = (char *)malloc(file_stat.st_size);
        if (file_content == NULL) {
            perror("Error allocating memory");
            fclose(file);

            struct MHD_Response *response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
            int ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
            MHD_destroy_response(response);
            return ret;
        }

        fread(file_content, 1, file_stat.st_size, file);
        fclose(file);

        const char *content_type;
        const char *file_extension = strrchr(url, '.');
        if (file_extension != NULL) {
            if (strcmp(file_extension, ".html") == 0) {
                content_type = "text/html";
            } else if (strcmp(file_extension, ".css") == 0) {
                content_type = "text/css";
            } else if (strcmp(file_extension, ".png") == 0) {
                content_type = "image/png";
            } else {
                content_type = "application/octet-stream";
            }
        } else {
            content_type = "text/plain";
        }

        struct MHD_Response *response = MHD_create_response_from_buffer(file_stat.st_size, file_content, MHD_RESPMEM_MUST_COPY);
        MHD_add_response_header(response, "Content-Type", content_type);
        int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);

        free(file_content);

        return ret;
    }
}

void request_completed(void *cls, struct MHD_Connection *connection,
                       void **con_cls, enum MHD_RequestTerminationCode toe) {
    if (cls != NULL) {
        struct ConnectionData *connection_data = (struct ConnectionData *)*con_cls;
        free(connection_data->index_page_text);
        free(connection_data);
        *con_cls = NULL;
    }
}
