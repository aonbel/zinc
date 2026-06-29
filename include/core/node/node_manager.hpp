#pragma once

#include "core/async/task.hpp"

namespace zinc::core::node {

using zinc::core::async::Task;

enum NodeStatus { Constructed, Initialized, Started, Paused, Destroyed };

class ManagedNode {
public:
    Task<void> Init();
    Task<void> Start();
    Task<void> Pause();
    Task<void> Destroy();

    NodeStatus GetNodeStatus() const;

protected:
    virtual Task<void> OnInit();
    virtual Task<void> OnStart();
    virtual Task<void> OnPause();
    virtual Task<void> OnDestroy();

private:
    std::atomic_int8_t node_status_;
};

class NodeManager {
public:
    Task<void> Init();
    Task<void> Run();
    Task<void> Destroy();

    void AddManagedNode(std::shared_ptr<ManagedNode> managed_node_ptr);

private:
    std::vector<std::shared_ptr<ManagedNode>> managed_nodes_ptrs_;
};
}  // namespace zinc::core::node