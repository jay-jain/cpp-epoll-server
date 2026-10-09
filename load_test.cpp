
#include <iostream>
#include <vector>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <csignal>

int main()
{
    const int NUM_CLIENTS = 10000;
    const int MESSAGES_PER_CLIENT = 1;

    signal(SIGPIPE, SIG_IGN);

    std::vector<int> client_fds;

    // STEP 1: Establish 10,000 connections.
    for (int i = 0; i < NUM_CLIENTS; i++)
    {
        int client_fd = socket(AF_INET, SOCK_STREAM, 0);

        if (client_fd == -1)
        {
            perror("Socket creation failed");
            continue;
        }

        sockaddr_in server_address{};

        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(8080);

        inet_pton(
            AF_INET,
            "127.0.0.1",
            &server_address.sin_addr
        );

        int result = connect(
            client_fd,
            reinterpret_cast<sockaddr*>(&server_address),
            sizeof(server_address)
        );

        if (result == -1)
        {
            perror("Connection failed");
            close(client_fd);
            continue;
        }

        client_fds.push_back(client_fd);

        if (client_fds.size() % 1000 == 0)
        {
            std::cout << client_fds.size()
                      << " clients connected\n";
        }
    }

    std::cout << "\nTotal connected clients: "
              << client_fds.size() << std::endl;

    // STEP 2: Send one message from each client.
    const std::string message = "Hello from load tester\n";

    unsigned long long successful_messages = 0;

    std::cout << "\nSending messages...\n";

    for (int client_fd : client_fds)
    {
        size_t total_sent = 0;
        bool send_failed = false;

        while (total_sent < message.size())
        {
            ssize_t bytes_sent = send(
                client_fd,
                message.data() + total_sent,
                message.size() - total_sent,
                0
            );

            if (bytes_sent > 0)
            {
                total_sent += bytes_sent;
            }
            else if (bytes_sent == -1 && errno == EINTR)
            {
                continue;
            }
            else
            {
                perror("Send failed");
                send_failed = true;
                break;
            }
        }

        if (!send_failed)
        {
            successful_messages++;
        }
    }

    std::cout << "\nSuccessful messages submitted: "
              << successful_messages << std::endl;

    std::cout << "Press Enter to disconnect all clients...\n";
    std::cin.get();

    // STEP 3: Disconnect all clients.
    for (int fd : client_fds)
    {
        close(fd);
    }

    return 0;
}
