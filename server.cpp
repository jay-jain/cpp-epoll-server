
#include <iostream>
#include <unordered_map>
#include <string>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

int main()
{
    std::cout << "Server starting...\n";

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1)
    {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    // Allow quick server restarts.
    int opt = 1;

    if (setsockopt(
            server_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &opt,
            sizeof(opt)
        ) == -1)
    {
        std::cerr << "Failed to set SO_REUSEADDR\n";
        close(server_fd);
        return 1;
    }

    // Make listening socket nonblocking.
    int flags = fcntl(server_fd, F_GETFL, 0);

    if (flags == -1 ||
        fcntl(server_fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        std::cerr << "Failed to make server socket nonblocking\n";
        close(server_fd);
        return 1;
    }

    sockaddr_in server_address{};

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8080);
    server_address.sin_addr.s_addr = INADDR_ANY;

    int result = bind(
        server_fd,
        reinterpret_cast<sockaddr*>(&server_address),
        sizeof(server_address)
    );

    if (result == -1)
    {
        std::cerr << "Failed to bind socket\n";
        close(server_fd);
        return 1;
    }

    std::cout << "Socket created and bound to port 8080.\n";

    if (listen(server_fd, 128) == -1)
    {
        std::cerr << "Failed to listen\n";
        close(server_fd);
        return 1;
    }

    std::cout << "Server listening on port 8080\n";

    // Create epoll instance.
    int epoll_fd = epoll_create1(0);

    if (epoll_fd == -1)
    {
        std::cerr << "Failed to create epoll instance\n";
        close(server_fd);
        return 1;
    }

    // Register listening socket.
    epoll_event event{};

    event.events = EPOLLIN;
    event.data.fd = server_fd;

    if (epoll_ctl(
            epoll_fd,
            EPOLL_CTL_ADD,
            server_fd,
            &event
        ) == -1)
    {
        std::cerr << "Failed to register listening socket\n";
        close(epoll_fd);
        close(server_fd);
        return 1;
    }

    epoll_event events[1024];

    unsigned long long total_messages = 0;

    std::unordered_map<int, std::string> pending_messages;

    // NEW: Throughput measurement.
    const unsigned long long TARGET_MESSAGES = 10000;

    using Clock = std::chrono::steady_clock;

    Clock::time_point start_time;
    bool timer_started = false;
    bool throughput_reported = false;

    while (true)
    {
        int ready_count = epoll_wait(
            epoll_fd,
            events,
            1024,
            -1
        );

        if (ready_count == -1)
        {
            if (errno == EINTR)
                continue;

            std::cerr << "epoll_wait failed\n";
            break;
        }

        for (int i = 0; i < ready_count; i++)
        {
            int ready_fd = events[i].data.fd;

            // New incoming connections.
            if (ready_fd == server_fd)
            {
                while (true)
                {
                    sockaddr_in client_addr{};
                    socklen_t client_addr_size = sizeof(client_addr);

                    int client_fd = accept(
                        server_fd,
                        reinterpret_cast<sockaddr*>(&client_addr),
                        &client_addr_size
                    );

                    if (client_fd == -1)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;

                        if (errno == EINTR)
                            continue;

                        std::cerr << "Failed to accept connection\n";
                        break;
                    }

                    // Make client socket nonblocking.
                    int client_flags = fcntl(client_fd, F_GETFL, 0);

                    if (client_flags == -1 ||
                        fcntl(
                            client_fd,
                            F_SETFL,
                            client_flags | O_NONBLOCK
                        ) == -1)
                    {
                        std::cerr << "Failed to make client nonblocking\n";
                        close(client_fd);
                        continue;
                    }

                    // Register client socket with epoll.
                    epoll_event client_event{};

                    client_event.events = EPOLLIN;
                    client_event.data.fd = client_fd;

                    if (epoll_ctl(
                            epoll_fd,
                            EPOLL_CTL_ADD,
                            client_fd,
                            &client_event
                        ) == -1)
                    {
                        std::cerr << "Failed to register client\n";
                        close(client_fd);
                        continue;
                    }

                    pending_messages[client_fd] = "";

                    std::cout << "Client connected: "
                              << client_fd << std::endl;
                }
            }

            // Existing client has activity.
            else
            {
                char buffer[1024];
                bool disconnect = false;

                while (true)
                {
                    ssize_t bytes_received = recv(
                        ready_fd,
                        buffer,
                        sizeof(buffer),
                        0
                    );

                    if (bytes_received > 0)
                    {
                        std::string& pending = pending_messages[ready_fd];

                        pending.append(buffer, bytes_received);

                        size_t newline_position;

                        while ((newline_position = pending.find('\n'))
                               != std::string::npos)
                        {
                            // Start timing at the first complete message.
                            if (!timer_started)
                            {
                                start_time = Clock::now();
                                timer_started = true;
                            }

                            total_messages++;

                            pending.erase(0, newline_position + 1);

                            if (total_messages % 1000 == 0)
                            {
                                std::cout
                                    << "Total messages received: "
                                    << total_messages
                                    << std::endl;
                            }

                            // NEW: Calculate throughput at 10,000 messages.
                            if (!throughput_reported &&
                                total_messages == TARGET_MESSAGES)
                            {
                                auto end_time = Clock::now();

                                double elapsed_seconds =
                                    std::chrono::duration<double>(
                                        end_time - start_time
                                    ).count();

                                double throughput =
                                    total_messages / elapsed_seconds;

                                std::cout << "\n===== THROUGHPUT RESULTS =====\n";

                                std::cout << "Messages received: "
                                          << total_messages << "\n";

                                std::cout << "Elapsed time: "
                                          << elapsed_seconds
                                          << " seconds\n";

                                std::cout << "Throughput: "
                                          << throughput
                                          << " messages/second\n";

                                std::cout << "==============================\n";

                                throughput_reported = true;
                            }
                        }
                    }
                    else if (bytes_received == 0)
                    {
                        disconnect = true;
                        break;
                    }
                    else
                    {
                        if (errno == EINTR)
                            continue;

                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;

                        std::cerr << "Failed to receive data\n";
                        disconnect = true;
                        break;
                    }
                }

                if (disconnect)
                {
                    pending_messages.erase(ready_fd);

                    std::cout << "Client disconnected: "
                              << ready_fd << std::endl;

                    epoll_ctl(
                        epoll_fd,
                        EPOLL_CTL_DEL,
                        ready_fd,
                        nullptr
                    );

                    close(ready_fd);
                }
            }
        }
    }

    close(epoll_fd);
    close(server_fd);

    return 0;
}
