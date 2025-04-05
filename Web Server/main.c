#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_METHOD_LEN 10
#define MAX_PATH_LEN 100

int visitor_count = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

char *url_decode(const char *encoded)
{
    size_t encoded_len = strlen(encoded);
    char *decoded = malloc(encoded_len + 1);
    size_t decoded_len = 0;

    // * Decode percent-encoded characters to hex
    for (size_t i = 0; i < encoded_len; i++)
    {
        // * Check if a '%' (followed by two integers) is found
        if (encoded[i] == '%' && i + 2 < encoded_len)
        {
            int hex_value;
            sscanf(encoded + i + 1, "%2x", &hex_value);
            decoded[decoded_len++] = hex_value;
            i += 2;
        }
        else decoded[decoded_len++] = encoded[i];
    }

    // * Add a null terminator
    decoded[decoded_len] = '\0';
    return decoded;
}

const char *get_file_extension(const char *file_name)
{
    const char *extension = strrchr(file_name, '.');

    // * If '.' is not found or is the start of the file name (hidden file)
    if (!extension || extension == file_name) return "";

    // * Return the extension (after '.')
    return extension + 1;
}

void get_date_and_time(char date_and_time[], size_t size)
{
    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    strftime(date_and_time, size, "%a, %d %b %Y %H:%M:%S GMT", tm_info);
}

void get_last_modified(const char *file_name, char *last_modified, size_t size)
{
    struct stat file_stat;
    if(stat(file_name, &file_stat) == 0)
    {
        struct tm *tm_info = gmtime(&file_stat.st_mtime);
        strftime(last_modified, size, "%a, %d %b %Y %H:%M:%S GMT", tm_info);
    }  
    else snprintf(last_modified, size, "Unknown");
}

long get_content_length(const char *file_name)
{
    struct stat file_stat;
    if(stat(file_name, &file_stat) == 0) return file_stat.st_size;
    else return -1;
}

const char *get_content_type(const char *file_extension)
{
    if (strcmp(file_extension, "html") == 0) return "text/html";
    if (strcmp(file_extension, "css") == 0) return "text/css";
    if (strcmp(file_extension, "js") == 0) return "application/javascript";
    if (strcmp(file_extension, "json") == 0) return "application/json";
    if (strcmp(file_extension, "png") == 0) return "image/png";
    if (strcmp(file_extension, "jpg") == 0 || strcmp(file_extension, "jpeg") == 0) return "image/jpeg";
    if (strcmp(file_extension, "gif") == 0) return "image/gif";
    if (strcmp(file_extension, "svg") == 0) return "image/svg+xml";
    if (strcmp(file_extension, "ico") == 0) return "image/x-icon";
    if (strcmp(file_extension, "pdf") == 0) return "application/pdf";
    if (strcmp(file_extension, "txt") == 0) return "text/plain";
    return "application/octet-stream";
}

int get_connection_type(const char *request, const char *http_version)
{
    int keep_alive = 0;
    if(strcmp(http_version, "HTTP/1.1") == 0)
    {
        keep_alive = 1;
        if(strstr(request, "Connection: close") != NULL) keep_alive = 0;
    }
    else if(strcmp(http_version, "HTTP/1.0") == 0)
    {
        if(strstr(request, "Connection: keep-alive") != NULL) keep_alive = 1;
    }
    return keep_alive;
}

int check_file(const char *file_path)
{
    FILE *fp = fopen(file_path, "r");
    
    if(fp == NULL)
    {
        perror("\nFile not found");
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

void send_file(int client_fd, const char *file_path)
{
    FILE *fp = fopen(file_path, "r");
    char file_buffer[BUFFER_SIZE];
    size_t bytes_read;
    
    if(fp == NULL)
    {
        perror("\nFile not found");
        return;
    }
    
    // * Send the file content in chunks
    while ((bytes_read = fread(file_buffer, 1, BUFFER_SIZE, fp)) > 0)
    {
        send(client_fd, file_buffer, bytes_read, 0);
    }
    fclose(fp);
}

void print_header(const char *label, char *header) {
    if(header) 
    {
        char value[256];  

        // * Read everything from after the first word until '\r\n'
        sscanf(header, "%*s %[^\r\n]", value);
        printf("%s: %s\n", label, value);
    }
}

void *handle_client(void *arg)
{
    int client_fd = *((int *)arg);
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));

    printf("\nRequest is being handled by thread [%lu].\n", (long)pthread_self());
    
    // * Receive request data from client and store into buffer
    ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);

    if(bytes_received > 0)
    {
        // * Null terminate the request
        buffer[bytes_received] = '\0';

        // * Extract important information from the request header
        char *user_agent = strstr(buffer, "User-Agent:");
        char *host = strstr(buffer, "Host:");
        char *accept_language = strstr(buffer, "Accept-Language:");
        char *accept_encoding = strstr(buffer, "Accept-Encoding:");
        char *connection = strstr(buffer, "Connection:");

        // * Extract the content length of the body
        int body_length = 0;
        char *length_ptr = strstr(buffer, "Content-Length: ");
        if (length_ptr) sscanf(length_ptr, "Content-Length: %d", &body_length);

        // * Find the start of the body
        char *body_start = strstr(buffer, "\r\n\r\n");

        // * Skip \r\n\r\n
        if (body_start) body_start += 4;
        else body_start = buffer + strlen(buffer);

        char *end_of_first_line = strchr(buffer, '\n');  
        if (end_of_first_line) *end_of_first_line = '\0';
        
        // * Print the (selected contents of the) request header
        printf("\n\n------ CLIENT REQUEST HEADER ------\n%s\n", buffer);
        print_header("User-Agent", user_agent);
        print_header("Host", host);
        print_header("Accept-Language", accept_language);
        print_header("Accept-Encoding", accept_encoding);
        print_header("Connection", connection);
        
        // * Parse the request line
        char method[MAX_METHOD_LEN], path[MAX_PATH_LEN], http_version[20];
        sscanf(buffer, "%s %s %s", method, path, http_version);
        
        // * Identify the request method
        char username[100] = "";
        if(strcmp(method, "POST") == 0)
        {
            strcpy(path, "/greetings");
    
            // * Read the request body
            int body_received = strlen(body_start);
            if (body_received < body_length)
            {
                read(client_fd, body_start + body_received, body_length - body_received);
                body_start[body_length] = '\0';
            }

            // * Extract the username and store it
            sscanf(body_start, "username=%99s", username);
            
            // * Print the body
            printf("\n\n------ CLIENT REQUEST BODY ------\n%s\n", body_start);
        }
        else if(strcmp(method, "GET") == 0)
        {   
            // * Redirect the homepage
            if(strcmp(path, "/") == 0 || strcmp(path, "/home") == 0) strcpy(path, "/index.html");
        }
        
        // * Decode the URL
        char *decoded_url = url_decode(path);
        
        // * Extract the file name from the path
        char file_name[100];
        strcpy(file_name, decoded_url + 1);

        // * Check if the requested file exists
        int file_exists = 1;
        char file_path[1024] = "public/";
        strcat(file_path, file_name);
        if(check_file(file_path) < 0)
        {
            strcpy(file_path, "public/notfound.html");
            file_exists = 0;
        }

        // * Get the current date and time
        char date_and_time[50];
        get_date_and_time(date_and_time, sizeof(date_and_time));

        // * Get the time the requested file was last modified
        char last_modified[50];
        get_last_modified(file_path, last_modified, sizeof(last_modified));

        // * Get the content length (size of the response body)
        long content_length = get_content_length(file_path);

        // * Get the content type
        char file_extension[32];
        strcpy(file_extension, get_file_extension(file_path));
        const char *content_type = get_content_type(file_extension);

        // * Get the connection type
        const char *connection_type = get_connection_type(buffer, http_version) ? "keep-alive" : "close";
        
        // * Construct the response
        printf("\n\nConstructing response...\n");
        char response[1000];
        char response_body[1000];

        if(file_exists || !strcmp(method, "POST"))
        {
            if(strcmp(path, "/greetings") == 0)
            {        
                int body_length = snprintf(response_body, sizeof(response_body), "<!DOCTYPE html>"
                                                                                 "<html lang='en'>"
                                                                                 "<head>"
                                                                                 "<meta charset='UTF-8'>"
                                                                                 "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                                                                                 "<title>Hi There!</title>"
                                                                                 "<link rel='icon' href='images/chatbubble.png' type='image/png'>"
                                                                                 "<link rel='preconnect' href='https://fonts.googleapis.com'>"
                                                                                 "<link rel='preconnect' href='https://fonts.gstatic.com' crossorigin>"
                                                                                 "<link href='https://fonts.googleapis.com/css2?family=Playwrite+US+Trad:wght@100..400&display=swap' rel='stylesheet'>"
                                                                                 "<link rel='stylesheet' href='styles.css'>"
                                                                                 "</head>"
                                                                                 "<body>"
                                                                                 "<div class='container'>"
                                                                                 "<h1 class='hidden'>Nice to meet you, %s!</h1>"
                                                                                 "</div>"
                                                                                 "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.7.1/jquery.min.js'></script>"
                                                                                 "<script>$('h1').fadeIn(1800);</script>"
                                                                                 "</body>"
                                                                                 "</html>", 
                username);

                char response[9000];
                int response_len = snprintf(response, sizeof(response), "HTTP/1.1 200 OK\r\n"
                                                                        "Content-Type: %s\r\n"
                                                                        "Content-Length: %d\r\n"
                                                                        "Connection: %s\r\n"
                                                                        "\r\n"
                                                                        "%s",
                content_type, body_length, connection_type, response_body);
                
                // * Send the response
                printf("\n\n------ SERVER RESPONSE ------\n%s\n", response);
                dprintf(client_fd, "%s", response);
                send(client_fd, response, body_length, 0);
            }
            else
            {
                snprintf(response, sizeof(response), "HTTP/1.1 200 OK\r\n"
                                                     "Date: %s\r\n"
                                                     "Server: My Server\r\n"
                                                     "Last-Modified: %s\r\n"
                                                     "Content-Length: %ld\r\n"
                                                     "Content-Type: %s\r\n"
                                                     "Connection: %s\r\n"
                                                     "\r\n", 
                date_and_time, last_modified, content_length, content_type, connection_type);
    
                // * Send the response header
                printf("\n\n------ SERVER RESPONSE ------\n%s\n", response);
                dprintf(client_fd, "%s", response);

                // * Send the response body
                send_file(client_fd, file_path);
    
                // * Increment visitor count
                if(strcmp(method, "GET") == 0 && strcmp(path, "/index.html") == 0)
                {
                    pthread_mutex_lock(&mutex);
                    printf("\nVisitor count: %d\n", ++visitor_count);
                    pthread_mutex_unlock(&mutex);
                }
            }
        }
        else
        {
            snprintf(response, sizeof(response), "HTTP/1.1 404 Not Found\r\n"
                                                 "Date: %s\r\n"
                                                 "Server: My Server\r\n"
                                                 "Last-Modified: %s\r\n"
                                                 "Content-Length: %ld\r\n"
                                                 "Content-Type: %s\r\n"
                                                 "Connection: %s\r\n"
                                                 "\r\n", 
            date_and_time, last_modified, content_length, content_type, connection_type);

            // * Send the response
            printf("\n\n------SERVER RESPONSE------\n%s\n", response);
            dprintf(client_fd, "%s", response);
            send_file(client_fd, file_path);
        }
    }
    close(client_fd);
    free(arg);
    free(buffer);
    return NULL;
}

int main(int argc, char const* argv[])
{
    int server_fd;
    struct sockaddr_in server_address;
    int opt = 1;

    // * Create server socket file descriptor
    printf("\nInitializing Server:\n  - Creating server socket...\n");
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if ((server_fd < 0))
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("  - Server socket created.\n");

    // * Configure socket options
    printf("  - Configurating server socket...\n");
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("  - Socket configuration failed");
        exit(EXIT_FAILURE);
    }
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);
    printf("  - Server socket configured.\n");

    // * Bind socket
    printf("  - Binding server socket...\n");
    if (bind(server_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0)
    {
        perror("  - Socket binding failed");
        exit(EXIT_FAILURE);
    }
    printf("  - Server socket bound.\n");

    // * Listen for incoming connections (max. 10 pending connections)
    if (listen(server_fd, 10) < 0)
    {
        perror("  - Socket failed to listen to connection request");
        exit(EXIT_FAILURE);
    }
    printf("\nServer listening on port %d...\n", PORT);
    
    // * Accept incoming requests
    printf("Waiting for incoming requests...\n\n");
    while (1)
    {
        struct sockaddr_in client_address;
        socklen_t client_addr_len = sizeof(client_address);
        int *client_fd = malloc(sizeof(int));

        if((*client_fd = accept(server_fd, (struct sockaddr *)&client_address, &client_addr_len)) < 0)
        {
            perror("Socket failed to accept connection request");
            continue;
        }

        // * Create a new thread to handle client request
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void *)client_fd);
        printf("\nRequest accepted.\n");
        pthread_detach(thread_id);
    }

    // * Close the server socket
    close(server_fd);
    return 0;
}
