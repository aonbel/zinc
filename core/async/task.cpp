#include "task.hpp"

template <class T>
void zinc::core::async::Task<T>::Resume() {
    handle_.promise().Resume();
}

template <class T>
bool zinc::core::async::Task<T>::Completed() {
    return handle_.promise().Completed();
}

template <class T>
zinc::core::async::WorkItem *zinc::core::async::Task<T>::WorkItem() {
    return &handle_.promise();
}

void zinc::core::async::Task<void>::Resume() {
    handle_.promise().Resume();
}

bool zinc::core::async::Task<void>::Completed() {
    return handle_.promise().Completed();
}

zinc::core::async::WorkItem *zinc::core::async::Task<void>::WorkItem() {
    return &handle_.promise();
}
