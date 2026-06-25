#pragma once

#include "../async/task.hpp"

namespace zinc::core::node {

using zinc::core::async::Task;

enum NodeStatus { Constructed, Initialized, Started, Paused, Destroyed };

class ManagedNode {
public:
    Task<void> Init() {
        auto node_status = node_status_.load();

        if (node_status != NodeStatus::Constructed) {
            std::cerr << "Node init attempt when prohibited\n";
            exit(EXIT_FAILURE);
        }

        node_status_.store(NodeStatus::Initialized);

        co_await OnInit();
    }

    Task<void> Start() {
        auto node_status = node_status_.load();

        if (node_status != NodeStatus::Initialized && node_status != NodeStatus::Paused) {
            std::cerr << "Node start attempt when prohibited\n";
            exit(EXIT_FAILURE);
        }

        node_status_.store(NodeStatus::Started);

        co_await OnStart();
    }

    Task<void> Pause() {
        auto node_status = node_status_.load();

        if (node_status != NodeStatus::Started) {
            std::cerr << "Node pause attempt when prohibited\n";
            exit(EXIT_FAILURE);
        }

        node_status_.store(NodeStatus::Paused);

        co_await OnPause();
    }

    Task<void> Destroy() {
        auto node_status = node_status_.load();

        if (node_status != NodeStatus::Paused) {
            std::cerr << "Node destroy attempt when prohibited\n";
            exit(EXIT_FAILURE);
        }

        node_status_.store(NodeStatus::Destroyed);

        co_await OnDestroy();
    }

    NodeStatus GetNodeStatus() const {
        return static_cast<NodeStatus>(node_status_.load());
    }

protected:
    virtual Task<void> OnInit() {
        co_return;
    }
    virtual Task<void> OnStart() {
        co_return;
    }
    virtual Task<void> OnPause() {
        co_return;
    }
    virtual Task<void> OnDestroy() {
        co_return;
    }

private:
    std::atomic_int8_t node_status_;
};

class NodeManager {
public:
    Task<void> Init() {
        for (auto& node_ptr : managed_nodes_ptrs_) {
            co_await node_ptr->Init();
        }
    }

    Task<void> Run() {
        for (auto& node_ptr : managed_nodes_ptrs_) {
            co_await node_ptr->Start();
        }
    }

    Task<void> Destroy() {
        for (auto& node_ptr : managed_nodes_ptrs_) {
            co_await node_ptr->Destroy();
        }
    }

    void AddManagedNode(std::shared_ptr<ManagedNode> managed_node_ptr) {
        managed_nodes_ptrs_.push_back(managed_node_ptr);
    }

private:
    std::vector<std::shared_ptr<ManagedNode>> managed_nodes_ptrs_;
};
}  // namespace zinc::core::node