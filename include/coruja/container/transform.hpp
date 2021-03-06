
// Copyright Ricardo Calheiros de Miranda Cosme 2018.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "coruja/container/container_view.hpp"
#include "coruja/support/type_traits.hpp"

#include <boost/hof/is_invocable.hpp>
#include <range/v3/begin_end.hpp>
#include <range/v3/distance.hpp>
#include <range/v3/range_traits.hpp>
#include <range/v3/view_facade.hpp>

#include <type_traits>

template<typename Rng, typename Transform>
class coruja_transform_view
  : public ranges::view_facade<coruja_transform_view<Rng, Transform>>
{
    template<typename F>
    struct invoke_observer_base : protected Transform
    {
        Transform& as_transform() noexcept
        { return static_cast<Transform&>(*this); }
        
        const Transform& as_transform() const noexcept
        { return static_cast<const Transform&>(*this); }
        
        invoke_observer_base(Transform t, F f)
            : Transform(std::move(t))
            , _f(std::move(f))
        {}
        
        F _f;
    };
    
    template<typename F>
    struct invoke_observer_impl : invoke_observer_base<F>
    {
        using base = invoke_observer_base<F>;
        using base::base;
        
        template<typename From, typename It>
        void operator()(From& from, It it) 
        {
            using namespace ranges;
            auto rng = coruja_transform_view{from, base::as_transform()};
            base::_f(rng, next(begin(rng), distance(begin(from), it)));
        }
    private:
        using invoke_observer_base<F>::operator();
    };
        
    template<typename F>
    struct invoke_observer_by_ref_impl : invoke_observer_base<F>
    {
        using base = invoke_observer_base<F>;
        using base::base;
        
        template<typename Ref>
        void operator()(Ref&& ref)
        { base::_f(base::as_transform()(std::forward<Ref>(ref))); }
    };
    
    friend ranges::range_access;
    
    Rng _rng;
    ranges::semiregular_t<Transform> _transform;
    
    class cursor
    {
        ranges::range_iterator_t<Rng> _it;
        mutable ranges::semiregular_t<Transform> _transform;
        
    public:
        
        cursor() = default;
        cursor(ranges::range_iterator_t<Rng> it, Transform transform)
            : _it(it)
            , _transform(std::move(transform))
        {}

        typename std::result_of<Transform(ranges::range_reference_t<Rng>)>::type
        read() const
        { return _transform(*_it); }
        
        bool equal(const cursor &that) const
        { return _it == that._it; }
        
        void next()
        { ++_it; }
        
        void prev()
        { --_it; }
        
        std::ptrdiff_t distance_to(const cursor& that) const
        { return ranges::distance(_it, that._it); }
        
        void advance(std::ptrdiff_t n)
        { ranges::advance(_it, n); }
    };
    
    cursor begin_cursor() const { return {_rng.begin(), _transform}; }
    cursor end_cursor() const { return {_rng.end(), _transform}; }

    template<typename F>
    invoke_observer_impl<typename std::decay<F>::type> invoke_observer(F&& f)
    { return {_transform, std::forward<F>(f)}; }
    
    template<typename F>
    invoke_observer_by_ref_impl<typename std::decay<F>::type> invoke_observer_by_ref(F&& f)
    { return {_transform, std::forward<F>(f)}; }
    
public:
    using observed_t = typename Rng::observed_t;
    using for_each_connection_t = typename Rng::for_each_connection_t;
    using before_erase_connection_t = typename Rng::before_erase_connection_t;

    //Deprecated
    using after_insert_connection_t = typename Rng::after_insert_connection_t;
    
    coruja_transform_view() = default;
    
    coruja_transform_view(Rng rng, Transform transform)
        : _rng(std::move(rng))
        , _transform(std::move(transform))
    {}

    const observed_t& observed() const noexcept
    { return _rng.observed(); }
        
    template<typename F>
    typename std::enable_if<
        !boost::hof::is_invocable<
            F,
            typename std::result_of<Transform(ranges::range_reference_t<Rng>)>::type
            >::value,
        for_each_connection_t
    >::type
    for_each(F&& f)
    { return _rng.for_each(invoke_observer(std::forward<F>(f))); }
    
    template<typename F>
    typename std::enable_if<
        boost::hof::is_invocable<
            F,
            typename std::result_of<Transform(ranges::range_reference_t<Rng>)>::type
            >::value,
        for_each_connection_t
    >::type for_each(F&& f)
    { return _rng.for_each(invoke_observer_by_ref(std::forward<F>(f))); }
    
    template<typename F>
    typename std::enable_if<
        !boost::hof::is_invocable<
            F,
            typename std::result_of<Transform(ranges::range_reference_t<Rng>)>::type
            >::value,
        before_erase_connection_t
    >::type
    before_erase(F&& f)
    { return _rng.before_erase(invoke_observer(std::forward<F>(f))); }
    
    template<typename F>
    typename std::enable_if<
        boost::hof::is_invocable<
            F,
            typename std::result_of<Transform(ranges::range_reference_t<Rng>)>::type
            >::value,
        before_erase_connection_t
    >::type before_erase(F&& f)
    { return _rng.before_erase(invoke_observer_by_ref(std::forward<F>(f))); }

    //Deprecated
    template<typename F>
    typename std::enable_if<
        !boost::hof::is_invocable<
            F,
            typename std::result_of<Transform(ranges::range_reference_t<Rng>)>::type
            >::value,
        after_insert_connection_t
    >::type
    after_insert(F&& f)
    { return _rng.after_insert(invoke_observer(std::forward<F>(f))); }
    
    //Deprecated
    template<typename F>
    typename std::enable_if<
        boost::hof::is_invocable<
            F,
            typename std::result_of<Transform(ranges::range_reference_t<Rng>)>::type
            >::value,
        after_insert_connection_t
    >::type after_insert(F&& f)
    { return _rng.after_insert(invoke_observer_by_ref(std::forward<F>(f))); }
};

namespace coruja {

template<typename ObservableErasableRange, typename F>
inline typename std::enable_if<
    is_observable_erasable_range<
        typename std::decay<ObservableErasableRange>::type>::value,
    coruja_transform_view<decltype(view(std::declval<ObservableErasableRange>())),
                          typename std::remove_reference<F>::type>
>::type
transform(ObservableErasableRange&& rng, F&& f)
{ return {view(rng), std::forward<F>(f)}; }
    
}
