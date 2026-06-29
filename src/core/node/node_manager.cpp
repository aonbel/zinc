#include "core/node/node_manager.hpp"

#include "core/async/task.hpp"

using zinc::core::async::Task;

Task<void> zinc::core::node::ManagedNode::Init() {
    auto node_status = node_status_.load();

    if (node_status != NodeStatus::Constructed) {
        std::cerr << "Node init attempt when prohibited\n";
        exit(EXIT_FAILURE);
    }

    node_status_.store(NodeStatus::Initialized);

    co_await OnInit();
}
Task<void> zinc::core::node::ManagedNode::Start() {
    auto node_status = node_status_.load();

    if (node_status != NodeStatus::Initialized && node_status != NodeStatus::Paused) {
        std::cerr << "Node start attempt when prohibited\n";
        exit(EXIT_FAILURE);
    }

    node_status_.store(NodeStatus::Started);

    co_await OnStart();
}
Task<void> zinc::core::node::ManagedNode::Pause() {
    auto node_status = node_status_.load();

    if (node_status != NodeStatus::Started) {
        std::cerr << "Node pause attempt when prohibited\n";
        exit(EXIT_FAILURE);
    }

    node_status_.store(NodeStatus::Paused);

    co_await OnPause();
}
Task<void> zinc::core::node::ManagedNode::Destroy() {
    auto node_status = node_status_.load();

    if (node_status != NodeStatus::Paused) {
        std::cerr << "Node destroy attempt when prohibited\n";
        exit(EXIT_FAILURE);
    }

    node_status_.store(NodeStatus::Destroyed);

    co_await OnDestroy();
}
zinc::core::node::NodeStatus zinc::core::node::ManagedNode::GetNodeStatus() const {
    return static_cast<NodeStatus>(node_status_.load());
}
Task<void> zinc::core::node::ManagedNode::OnInit() {
    co_return;
}
Task<void> zinc::core::node::ManagedNode::OnStart() {
    co_return;
}
Task<void> zinc::core::node::ManagedNode::OnPause() {
    co_return;
}
Task<void> zinc::core::node::ManagedNode::OnDestroy() {
    co_return;
}
Task<void> zinc::core::node::NodeManager::Init() {
    for (auto& node_ptr : managed_nodes_ptrs_) {
        co_await node_ptr->Init();
    }
}
Task<void> zinc::core::node::NodeManager::Run() {
    for (auto& node_ptr : managed_nodes_ptrs_) {
        co_await node_ptr->Start();
    }
}
Task<void> zinc::core::node::NodeManager::Destroy() {
    for (auto& node_ptr : managed_nodes_ptrs_) {
        co_await node_ptr->Destroy();
    }
}
void zinc::core::node::NodeManager::AddManagedNode(std::shared_ptr<ManagedNode> managed_node_ptr) {
    managed_nodes_ptrs_.push_back(managed_node_ptr);
}
