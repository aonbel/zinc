#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <asm-generic/socket.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>

#include "core/async/task.hpp"
#include "core/utility/meta.hpp"
#include "core/node/node.hpp"
#include "core/async/awaiters/duration.hpp"

namespace zinc::nodes::epoll_node {

using namespace std::chrono_literals;

using zinc::core::async::SubmitToGlobalThreadPool;
using zinc::core::async::Task;
using zinc::core::async::ThreadPool;
using zinc::core::meta::ClassList;
using zinc::core::node::NodeStatus;

struct ClientDataT {
    int32_t fd;
    sockaddr_in addr;
    std::vector<uint8_t> data;
};

// TODO add move, copy, and pointer inlets and outlets support

struct EpollNode : zinc::core::node::Node<ClassList<ClientDataT>, ClassList<ClientDataT>> {

    struct ClientContextT {
        sockaddr_in addr;

        std::queue<std::vector<uint8_t>> data;
    };

    EpollNode(uint16_t port, int backlog = 5, int epoll_flags = 0,
              size_t epoll_events_buffer_size = 64, int epoll_timeout = 0,
              size_t epollin_event_buffer_size = 1024,
              std::chrono::milliseconds epoll_event_loop_delay = 0ms, int reuseaddr = 1,
              int reuseport = 1)
        : port_(port),
          backlog_(backlog),
          epoll_flags_(epoll_flags),
          epoll_events_buffer_size_(epoll_events_buffer_size),
          epoll_timeout_(epoll_timeout),
          epollin_event_buffer_size_(epollin_event_buffer_size),
          epoll_event_loop_delay_(epoll_event_loop_delay),
          reuseaddr_(reuseaddr),
          reuseport_(reuseport) {
        if (epoll_events_buffer_size == 0) {
            std::cerr << "events buffer size should be positive\n";
            exit(EXIT_FAILURE);
        }

        epoll_events_buffer_.resize(epoll_events_buffer_size_);

        if (epollin_event_buffer_size == 0) {
            std::cerr << "epollin event buffer size should be positive\n";
            exit(EXIT_FAILURE);
        }
    }

    Task<void> OnInit() override {
        main_socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);

        if (main_socket_fd_ < 0) {
            std::cerr << "unable to create main socket[" << errno << "]\n";
            exit(EXIT_FAILURE);
        }

        if (reuseaddr_ && setsockopt(main_socket_fd_, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_,
                                     sizeof(reuseaddr_)) < 0) {

            std::cerr << "unable to set reuseaddr option to main socket[" << errno << "]\n";
            exit(EXIT_FAILURE);
        }

        if (reuseport_ && setsockopt(main_socket_fd_, SOL_SOCKET, SO_REUSEPORT, &reuseport_,
                                     sizeof(reuseport_)) < 0) {

            std::cerr << "unable to set reuseport option to main socket[" << errno << "]\n";
            exit(EXIT_FAILURE);
        }

        memset(&main_socket_addr_, 0, sizeof(main_socket_addr_));

        main_socket_addr_.sin_family = AF_INET;
        main_socket_addr_.sin_addr.s_addr = INADDR_ANY;
        main_socket_addr_.sin_port = htons(port_);

        if (bind(main_socket_fd_, reinterpret_cast<const sockaddr *>(&main_socket_addr_),
                 sizeof(main_socket_addr_)) < 0) {
            std::cerr << "unable to name main socket[" << errno << "]\n";
            close(main_socket_fd_);
            exit(EXIT_FAILURE);
        }

        epoll_fd_ = epoll_create1(epoll_flags_);

        if (epoll_fd_ < 0) {
            std::cerr << "unable to create epoll[" << errno << "]\n";
            close(main_socket_fd_);
            exit(EXIT_FAILURE);
        }

        epoll_event main_socket_event;
        main_socket_event.events = EPOLLIN;
        main_socket_event.data.fd = main_socket_fd_;

        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, main_socket_fd_, &main_socket_event);

        if (listen(main_socket_fd_, backlog_) < 0) {
            std::cerr << "unable to listen main socket[" << errno << "]\n";
            close(main_socket_fd_);
            exit(EXIT_FAILURE);
        }

        co_return;
    }

    Task<void> OnStart() override {
        auto loop = EventLoop();

        SubmitToGlobalThreadPool(loop);

        co_return;
    }

    Task<void> operator()(ClientDataT data) override {
        auto &client_context = client_fd_to_context_[data.fd];

        {
            std::unique_lock lg(messages_mutex_);

            client_context.data.push(std::move(data.data));
        }

        epoll_event client_socket_event;
        client_socket_event.events = EPOLLIN | EPOLLOUT;
        client_socket_event.data.fd = data.fd;

        epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, data.fd, &client_socket_event);

        co_return;
    }

private:
    Task<void> EventLoop() {
        while (GetNodeStatus() == NodeStatus::Started) {
            int num_events = epoll_wait(epoll_fd_, epoll_events_buffer_.data(),
                                        epoll_events_buffer_size_, epoll_timeout_);

            for (int i = 0; i < num_events; i++) {
                epoll_event &event = epoll_events_buffer_[i];
                if (event.data.fd == main_socket_fd_ && event.events & EPOLLIN) {
                    sockaddr_in client_addr;
                    socklen_t client_addr_len = sizeof(client_addr);
                    memset(&client_addr, 0, sizeof(client_addr));
                    int client_fd =
                        accept(main_socket_fd_, reinterpret_cast<sockaddr *>(&client_addr),
                               &client_addr_len);

                    if (client_fd < 0) {
                        std::cerr << "unable to create client socket[" << errno << "]\n";
                    }

                    int client_fd_flags = fcntl(client_fd, F_GETFL);
                    fcntl(client_fd, F_SETFL, client_fd_flags | O_NONBLOCK);

                    client_fd_to_context_.emplace(client_fd, ClientContextT{client_addr, {}});

                    epoll_event client_socket_event;
                    client_socket_event.events = EPOLLIN;
                    client_socket_event.data.fd = client_fd;

                    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &client_socket_event);
                } else if (event.events & EPOLLIN) {
                    ClientDataT client_data_buffer{
                        event.data.fd, client_fd_to_context_[event.data.fd].addr,
                        std::vector<uint8_t>(epollin_event_buffer_size_)};

                    ssize_t data_size = read(event.data.fd, client_data_buffer.data.data(),
                                             epollin_event_buffer_size_);

                    if (data_size == -1) {
                        std::cerr << "unable to read from client fd [" << errno << "]\n";
                    }

                    if (data_size == 0) {
                        client_fd_to_context_.erase(event.data.fd);
                        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, event.data.fd, nullptr);
                        close(event.data.fd);

                        continue;
                    }

                    auto next = Next(client_data_buffer);

                    SubmitToGlobalThreadPool(next);
                } else if (event.events & EPOLLOUT) {
                    int client_fd = event.data.fd;

                    auto &context = client_fd_to_context_[client_fd];

                    {
                        std::unique_lock lg(messages_mutex_);

                        auto &data_to_write = context.data.front();

                        size_t written_size =
                            write(client_fd, data_to_write.data(), data_to_write.size());

                        if (written_size < data_to_write.size()) {
                            data_to_write.erase(data_to_write.begin(),
                                                data_to_write.begin() + written_size);
                        } else {
                            context.data.pop();
                        }

                        if (context.data.empty()) {
                            epoll_event client_socket_event;
                            client_socket_event.events = EPOLLIN;
                            client_socket_event.data.fd = client_fd;

                            epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_fd, &client_socket_event);
                        }
                    }
                } else {
                    std::cerr << "unknown event\n";
                }
            }

            co_await epoll_event_loop_delay_;
        }

        co_return;
    }

    int main_socket_fd_;
    sockaddr_in main_socket_addr_;

    int epoll_fd_;
    std::vector<epoll_event> epoll_events_buffer_;
    std::mutex messages_mutex_;

    std::unordered_map<int, ClientContextT> client_fd_to_context_;

    const uint16_t port_;
    const int backlog_;
    const int epoll_flags_;
    const size_t epoll_events_buffer_size_;
    const int epoll_timeout_;
    const size_t epollin_event_buffer_size_;
    const std::chrono::milliseconds epoll_event_loop_delay_;
    const int reuseaddr_;
    const int reuseport_;
};
}  // namespace zinc::nodes::epoll_node