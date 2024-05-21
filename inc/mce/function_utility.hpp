//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
/**
 @file function_utility.hpp 
 Utilities for dealing with functions.
 */
#ifndef __MERCURY_COROUTINE_ENGINE_FUNCTION_UTILITY__
#define __MERCURY_COROUTINE_ENGINE_FUNCTION_UTILITY__

// c++
#include <type_traits>
#include <functional>

namespace mce {
namespace detail {

template <typename T>
using unqualified = typename std::decay<T>::type;

// handle pre and post c++17 
#if __cplusplus >= 201703L
template <typename F, typename... Ts>
using function_return_type = typename std::invoke_result<unqualified<F>,Ts...>::type;
#else 
template <typename F, typename... Ts>
using function_return_type = typename std::result_of<unqualified<F>(Ts...)>::type;
#endif

// Convert void type to int
template <typename T>
struct convert_void_
{
    typedef T type;
};

template <>
struct convert_void_<void>
{
    typedef int type;
};

template <typename F, typename... A>
using convert_void_return = typename convert_void_<function_return_type<F,A...>>::type;

template<typename T, typename _ = void>
struct is_container : std::false_type {};

template<typename... Ts>
struct is_container_helper {};

template< bool B, class T, class F >
using conditional_t = typename std::conditional<B,T,F>::type;

template<typename T>
struct is_container<
        T,
        conditional_t<
            false,
            is_container_helper<
                typename T::value_type,
                typename T::iterator,
                decltype(std::declval<T>().begin()),
                decltype(std::declval<T>().end())
                >,
            void
            >
        > : public std::true_type {};

}

/// thunk type definition. Also known as a nullary function
using thunk = std::function<void()>;

namespace detail {

/// std::bind arguments to an std::function
template <typename F, typename... A>
std::function<detail::function_return_type<F,A...>()> 
wrap_args(F&& func, A&&... args) 
{
    return std::bind(std::forward<F>(func), std::forward<A>(args)...);
}

// implicitly convertable to thunk
inline thunk wrap_return(std::true_type, thunk th)
{
    return th;
}

// fallback template to wrap Callable with non-void return type
template <typename Callable>
thunk wrap_return(std::false_type, Callable&& cb) 
{
    // Convertable to thunk whose 'inner' procedure can be rvalue constructed.
    // This is a workaround to a limitation in c++11 lambdas which don't have an 
    // obvious mechanism for rvalue copy capturing
    struct wrapper 
    {
        inline void operator()(){ inner(); } // ignores return type
        Callable inner; // move or copy constructable
    };

    return wrapper{ std::forward<Callable>(cb) };
}

}

// handle case where Callable is passed no bind arguments
template <typename Callable>
thunk make_thunk(Callable&& cb) 
{
    // determine return type
    using isv = typename std::is_void<detail::function_return_type<Callable>>;
    return detail::wrap_return(
        std::integral_constant<bool,isv::value>(),
        std::forward<Callable>(cb));
}

/*
Allows any input function (assuming any required input arguments are passed to 
this procedure) to be converted to a thunk, abstracting away the function's 
behavior. 
*/
template <typename Callable, typename A, typename... As>
thunk make_thunk(Callable&& cb, A&& a, As&&... as) 
{
    return make_thunk(
        // convert to a Callable with no arguments
        detail::wrap_args(
            std::forward<Callable>(cb),
            std::forward<A>(a), 
            std::forward<As>(as)...));
}

}

#endif
