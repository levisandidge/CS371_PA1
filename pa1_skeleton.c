/*
# Copyright 2025 University of Kentucky
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
*/

/* 
Please specify the group members here
# Student #1: Brett Carson
# Student #2: Levi Sandidge
# Student #3: Leighanne Lyvers
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/types.h>

#define MAX_EVENTS 64
#define MESSAGE_SIZE 16
#define DEFAULT_CLIENT_THREADS 4

char *server_ip = "127.0.0.1";
int server_port = 12345;
int num_client_threads = DEFAULT_CLIENT_THREADS;
int num_requests = 1000000;

/*
 * This structure is used to store per-thread data in the client
 */
typedef struct {
    int epoll_fd;
    int socket_fd;
    long long total_rtt;
    long total_messages;
    float request_rate;
} client_thread_data_t;

/*
 * This function runs in a separate client thread to handle communication with the server
 */
void *client_thread_func(void *arg) {
    client_thread_data_t *data = (client_thread_data_t *)arg;
    struct epoll_event event, events[MAX_EVENTS];
    char send_buf[MESSAGE_SIZE] = "ABCDEFGHIJKLMNOP"; 
    char recv_buf[MESSAGE_SIZE];
    struct timeval start, end;



    /* TODO:
     * It sends messages to the server, waits for a response using epoll,
     * and measures the round-trip time (RTT) of this request-response.
     */

    // Monitor events for incoming data, associated with corresponding socket, and register the "connected"
    // client_thread's socket in its epoll instance
    event.events = EPOLLIN;
    event.data.fd = data->socket_fd;
    epoll_ctl(data->epoll_fd, EPOLL_CTL_ADD, data->socket_fd, &event);

    // Report that a client thread has started, and the number of requests it will send
    printf("Client thread started, sending %d requests...\n", num_requests);

    // For each request from the client to the server,
    for (int i = 0; i < num_requests; i++) 
    {
        // Record timestamp of start of request so we can calculate RTT later, then send the message
        gettimeofday(&start, NULL);
        send(data->socket_fd, send_buf, MESSAGE_SIZE, 0);
        printf("Client thread sent message %d\n", i + 1);

        // Wait for a response from the server, and if the response is from the correct socket,
        int n = epoll_wait(data->epoll_fd, events, MAX_EVENTS, -1);
        if (n > 0 && events[0].data.fd == data->socket_fd) 
        {
            // While there are still more bytes to receive from the server (16 total)
            int bytes_received = 0;
            while (bytes_received < MESSAGE_SIZE) 
            {
                // Receive data using recv function with parameters of the corresponding socket, a pointer to
                // the index in the buffer where data wil be stored, the max number of bytes, and 0 (no special options)
                int ret = recv(data->socket_fd, recv_buf + bytes_received, MESSAGE_SIZE - bytes_received, 0);

                // If there is still data to receive, append to received message
                if (ret > 0) 
                {
                    bytes_received += ret;
                } 
                // If no data was received, connection with the server must have failed
                else if (ret == 0) 
                {
                    printf("Server closed connection unexpectedly.\n");
                    break;
                } 
                // Otherwise, the call to recv function failed
                else 
                {
                    perror("recv() failed");
                    break;
                }
            }

            // Obtain end timestamp for purpose of calculating round-trip time
            gettimeofday(&end, NULL);

            // Calculate the RTT in microseconds (the time for the message to go from client to server back to client)
            long long rtt = (end.tv_sec - start.tv_sec) * 1000000LL + (end.tv_usec - start.tv_usec);

            // Add the RTT to the total RTT for the request and increment the message counter
            data->total_rtt += rtt;
            data->total_messages++;

            // Print statement to report that the round-trip has been completed (client received message)
            printf("Client thread received message %d, RTT = %lld us\n", i + 1, rtt);
        }
    }

    /* TODO:
     * The function exits after sending and receiving a predefined number of messages (num_requests). 
     * It calculates the request rate based on total messages and RTT
     */

    // Now that all requests from client to server have been made, we can calculate request rate 
    // If at least one message was sent, calculate request rate
    if (data->total_messages > 0) 
    {
        // Request rate = number of messages divided by total RTT in seconds (or 0 if total_RTT = 0)
        if (data->total_rtt > 0) 
        {
            data->request_rate = (double)data->total_messages / (data->total_rtt / 1000000.0);
        } 
        else 
        {
            data->request_rate = 0.0;
        }
        
    } 
    else 
    {
        // If no messages were sent, the request rate is 0
        data->request_rate = 0;
    }

    // All messages have been sent, so we can close the socket and report client thread request rate
    close(data->socket_fd);
    close(data->epoll_fd);
    printf("Client thread finished with request rate = %.2f messages/s\n", data->request_rate);
    return NULL;
}


/*
 * This function orchestrates multiple client threads to send requests to a server,
 * collect performance data of each threads, and compute aggregated metrics of all threads.
 */
void run_client() 
{
    // Set up the client threads that will send requests to the server
    pthread_t threads[num_client_threads];
    client_thread_data_t thread_data[num_client_threads];
    struct sockaddr_in server_addr;

    // Set up structure for server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    // Report that the client is connecting to the server and give server details
    printf("Client connecting to server %s:%d...\n", server_ip, server_port);

    // For each thread,
    for (int i = 0; i < num_client_threads; i++) 
    {
        // Establish TCP connection with the server and register socket with epoll
        thread_data[i].socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        thread_data[i].epoll_fd = epoll_create1(0);

        // Initialize relevant data for each thread
        thread_data[i].total_rtt = 0;
        thread_data[i].total_messages = 0;
        thread_data[i].request_rate = 0.0;

        // Establish a connection between the client's socket and the server using server address
        connect(thread_data[i].socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

        // Create a new thread to pass thread-specific data
        pthread_create(&threads[i], NULL, client_thread_func, &thread_data[i]);
    }

    // Initialize data for total RTT, messages, and request rate (as opposed to thread-specific data)
    long long total_rtt = 0;
    long total_messages = 0;
    float total_request_rate = 0.0;

    // For each client thread,
    for (int i = 0; i < num_client_threads; i++) 
    {
        // Wait for all threads to finish so we can calculate data across all threads
        pthread_join(threads[i], NULL);

        // Calculate the total RTT, messages, and request rate by accumulating across all threads
        total_rtt += thread_data[i].total_rtt;
        total_messages += thread_data[i].total_messages;
        total_request_rate += thread_data[i].request_rate;
    }

    // Report all results
    printf("Client completed. Aggregated results:\n");
    printf("Average RTT: %lld us\n", total_messages > 0 ? total_rtt / total_messages : 0);
    printf("Total Request Rate: %f messages/s\n", total_request_rate);
}

/*
 * Server function to handle client connections and echo messages.
 */
void run_server() 
{
    // Run the server, and report server-specific details
    printf("Server started on %s:%d\n", server_ip, server_port);

    // Create structures to store the IP address and port number of the server and client
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    // Create integers to store the file descriptors for the listening socket and epoll instance
    int listen_fd, epoll_fd;

    // Create TCP socket for IPv4 communication, which will correspond to listening socket fd
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    // Create epoll instance to monitor the created file descriptors
    epoll_fd = epoll_create1(0);

    // Create structure of server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;

    // Server accepts connections from any address (also set the server port)
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);

    // Bind the listening socket to corresponding server address, then listen for connections
    bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(listen_fd, SOMAXCONN);

    // Use epoll and listen to wait for incoming data
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = listen_fd;

    // Register listen_fd with created epoll instance
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &event);

    // Report that the server is awaiting a connection with a client
    printf("Server listening for connections...\n");

    // Infinite loop to handle events,
    while (1) 
    {
        // Use epoll to wait for upcoming event
        struct epoll_event events[MAX_EVENTS];
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        // For each event returned by epoll_wait,
        for (int i = 0; i < n; i++) 
        {
            // If the event is on the listening socket,
            if (events[i].data.fd == listen_fd) 
            {
                // Determine length of client, and accept connection
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);

                // Wait for data from client using client_fd
                struct epoll_event client_event;
                client_event.events = EPOLLIN;
                client_event.data.fd = client_fd;

                // Register client file descriptor with epoll instance
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event);

                // Report new client connection
                printf("Server accepted new client connection.\n");
            }
            else 
            {
                // Handle incoming data from client, echo received data back to the client
                char buf[MESSAGE_SIZE];
                int ret = recv(events[i].data.fd, buf, MESSAGE_SIZE, 0);
                if (ret > 0) 
                {
                    // Echo data received from client back to client
                    send(events[i].data.fd, buf, ret, 0);
                }
                else 
                {
                    // If client closed connection, close corresponding socket and remove from epoll
                    close(events[i].data.fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                }
            }
        }
    }
}


int main(int argc, char *argv[]) 
{
    if (argc > 1 && strcmp(argv[1], "server") == 0) 
    {
        if (argc > 2) server_ip = argv[2];
        if (argc > 3) server_port = atoi(argv[3]);

        run_server();
    } 
    else if (argc > 1 && strcmp(argv[1], "client") == 0) 
    {
        if (argc > 2) server_ip = argv[2];
        if (argc > 3) server_port = atoi(argv[3]);
        if (argc > 4) num_client_threads = atoi(argv[4]);
        if (argc > 5) num_requests = atoi(argv[5]);

        run_client();
    } 
    else 
    {
        printf("Usage: %s <server|client> [server_ip server_port num_client_threads num_requests]\n", argv[0]);
    }

    return 0;
}
