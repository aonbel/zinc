#include <memory>
#include "core/async/thread_pool_context.hpp"
#include "core/node/node_manager.hpp"
#include "nodes/epoll_node/epoll_node.hpp"

using namespace std::chrono_literals;
using zinc::core::async::SubmitToGlobalThreadPool;
using zinc::core::async::Task;
using zinc::core::async::ThreadPoolContext;
using zinc::core::node::NodeManager;
using zinc::nodes::epoll_node::EpollNode;

NodeManager node_manager;

Task<void> MainTask() {
    co_await node_manager.Init();
    co_await node_manager.Run();
}

int main() {
    ThreadPoolContext ctx{4};
    auto epoll_node_ptr = std::make_shared<EpollNode>(8080);
    node_manager.AddManagedNode(epoll_node_ptr);

    epoll_node_ptr->AddSubscriber<zinc::nodes::epoll_node::ClientDataT*>(epoll_node_ptr);

    SubmitToGlobalThreadPool(MainTask().WorkItem());

    std::this_thread::sleep_for(300s);
}