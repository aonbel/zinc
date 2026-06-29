#pragma once

#include <memory>
#include "core/async/task.hpp"
#include "core/utility/meta.hpp"
#include "node_manager.hpp"

namespace zinc::core::node {

using zinc::core::async::Task;
using zinc::core::meta::ClassList;
using zinc::core::meta::ClassPair;
using zinc::core::meta::IsIn;
using zinc::core::meta::ProvideFunctor;
using zinc::core::meta::ProvideFunctors;

template <class Inlets = ClassList<>, class Outlets = ClassList<>>
class Node;

template <class... Inlets, class... Outlets>
class Node<ClassList<Inlets...>, ClassList<Outlets...>>
    : public ProvideFunctors<ClassList<ClassPair<Task<void>, ClassList<Inlets>>...>>,
      public ManagedNode {
    template <class Outlet>
    struct OutletManager {
        void AddSubscriber(
            std::shared_ptr<ProvideFunctor<Task<void>, ClassList<Outlet>>> subscriber_ptr) {
            subscriber_inlet_ptrs_.push_back(subscriber_ptr);
        }

        Task<void> Invoke(Outlet data_ptr) {
            for (auto subscriber_inlet_ptr : subscriber_inlet_ptrs_) {
                co_await (*subscriber_inlet_ptr)(data_ptr);
            }
        }

    private:
        std::vector<std::shared_ptr<ProvideFunctor<Task<void>, ClassList<Outlet>>>>
            subscriber_inlet_ptrs_;
    };

public:
    template <class Outlet>
    void AddSubscriber(
        std::shared_ptr<ProvideFunctor<Task<void>, ClassList<Outlet>>> subscriber_ptr) {
        auto &outlet_manager = std::get<OutletManager<Outlet>>(outlet_managers_);

        outlet_manager.AddSubscriber(subscriber_ptr);
    };

protected:
    template <class Outlet>
        requires(IsIn<Outlet, ClassList<Outlets...>>::value)
    Task<void> Next(Outlet data_ptr) {
        auto &outlet_manager = std::get<OutletManager<Outlet>>(outlet_managers_);

        co_await outlet_manager.Invoke(data_ptr);
    };

private:
    std::tuple<OutletManager<Outlets>...> outlet_managers_;
};
}  // namespace zinc::core::node