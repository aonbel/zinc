#pragma once

#include <tuple>
#include <type_traits>

namespace zinc::core::meta {
template <class... Classes>
using ClassList = std::tuple<Classes...>;

template <class First, class Second>
struct ClassPair : public ClassList<First, Second> {
    using FirstT = First;
    using SecondT = Second;
};

template <class ReturnType, class Arguments>
struct ProvideFunctor;

template <class ReturnType, class... Args>
struct ProvideFunctor<ReturnType, ClassList<Args...>> {
    virtual ReturnType operator()(Args... arguments) = 0;
};

template <class DefinitionsList>
struct ProvideFunctors;

template <class ReturnType, class... Args, class... OtherDefinitions>
struct ProvideFunctors<ClassList<ClassPair<ReturnType, ClassList<Args...>>, OtherDefinitions...>>
    : public ProvideFunctor<ReturnType, ClassList<Args...>>,
      public ProvideFunctors<ClassList<OtherDefinitions...>> {};

template <>
struct ProvideFunctors<ClassList<>> {};

template <class Class, class ClassList>
struct IsIn;

template <class Class, class... OthersInClassList>
struct IsIn<Class, ClassList<Class, OthersInClassList...>> : std::true_type {};

template <class Class, class FirstInClassList, class... OthersInClassList>
struct IsIn<Class, ClassList<FirstInClassList, OthersInClassList...>>
    : IsIn<Class, ClassList<OthersInClassList...>> {};

template <class Class>
struct IsIn<Class, ClassList<>> : std::false_type {};

template <class Class, class ClassList>
using IsInV = typename IsIn<Class, ClassList>::value;

}  // namespace zinc::core::meta
