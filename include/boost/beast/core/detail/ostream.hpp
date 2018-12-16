//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_DETAIL_OSTREAM_HPP
#define BOOST_BEAST_DETAIL_OSTREAM_HPP

#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/buffers_range.hpp>
#include <boost/beast/core/read_size.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <memory>
#include <iosfwd>
#include <streambuf>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast {
namespace detail {

template<class Buffers>
class buffers_helper
{
    Buffers b_;

public:
    explicit
    buffers_helper(Buffers const& b)
        : b_(b)
    {
    }

    template<class B>
    friend
    std::ostream&
    operator<<(std::ostream& os,
        buffers_helper<B> const& v);
};

template<class Buffers>
std::ostream&
operator<<(std::ostream& os,
    buffers_helper<Buffers> const& v)
{
    for(auto b : buffers_range(std::ref(v.b_)))
        os.write(static_cast<char const*>(b.data()), b.size());
    return os;
}

//------------------------------------------------------------------------------

struct basic_streambuf_movable_helper :
    std::basic_streambuf<char, std::char_traits<char>>
{
    basic_streambuf_movable_helper(
        basic_streambuf_movable_helper&&) = default;
};

using basic_streambuf_movable =
    std::is_move_constructible<basic_streambuf_movable_helper>;

//------------------------------------------------------------------------------

template<class DynamicBuffer,
    class CharT, class Traits, bool isMovable>
class ostream_buffer;

template<class DynamicBuffer, class CharT, class Traits>
class ostream_buffer
        <DynamicBuffer, CharT, Traits, true>
    : public std::basic_streambuf<CharT, Traits>
{
    using int_type = typename
        std::basic_streambuf<CharT, Traits>::int_type;

    using traits_type = typename
        std::basic_streambuf<CharT, Traits>::traits_type;

    DynamicBuffer& b_;

public:
    ostream_buffer(ostream_buffer&&) = default;
    ostream_buffer(ostream_buffer const&) = delete;

    ~ostream_buffer() noexcept
    {
        sync();
    }

    explicit
    ostream_buffer(DynamicBuffer& b)
        : b_(b)
    {
        prepare();
    }

    int_type
    overflow(int_type ch) override
    {
        if(! Traits::eq_int_type(ch, Traits::eof()))
        {
            Traits::assign(*this->pptr(),
                static_cast<CharT>(ch));
            flush(1);
            prepare();
            return ch;
        }
        flush();
        return traits_type::eof();
    }

    int
    sync() override
    {
        flush();
        prepare();
        return 0;
    }

private:
    void
    prepare()
    {
        static std::size_t constexpr max_size = 65536;
        auto const max_prepare = std::min<std::size_t>(
            std::max<std::size_t>(
                512, b_.capacity() - b_.size()),
            std::min<std::size_t>(
                max_size, b_.max_size() - b_.size()));
        auto const bs = b_.prepare(max_prepare);
        auto const b = buffers_front(bs);
        auto const p = static_cast<CharT*>(b.data());
        this->setp(p,
            p + b.size() / sizeof(CharT) - 1);
    }

    void
    flush(int extra = 0)
    {
        b_.commit(
            (this->pptr() - this->pbase() + extra) *
                sizeof(CharT));
    }
};

// This nonsense is all to work around a glitch in libstdc++
// where std::basic_streambuf copy constructor is private:
// https://github.com/gcc-mirror/gcc/blob/gcc-4_8-branch/libstdc%2B%2B-v3/include/std/streambuf#L799

template<class DynamicBuffer, class CharT, class Traits>
class ostream_buffer
        <DynamicBuffer, CharT, Traits, false>
    : public std::basic_streambuf<CharT, Traits>
{
    using int_type = typename
        std::basic_streambuf<CharT, Traits>::int_type;

    using traits_type = typename
        std::basic_streambuf<CharT, Traits>::traits_type;

    DynamicBuffer& b_;

public:
    ostream_buffer(ostream_buffer&&) = delete;
    ostream_buffer(ostream_buffer const&) = delete;

    ~ostream_buffer() noexcept
    {
        sync();
    }

    explicit
    ostream_buffer(DynamicBuffer& b)
        : b_(b)
    {
        prepare();
    }

    int_type
    overflow(int_type ch) override
    {
        if(! Traits::eq_int_type(ch, Traits::eof()))
        {
            Traits::assign(*this->pptr(),
                static_cast<CharT>(ch));
            flush(1);
            prepare();
            return ch;
        }
        flush();
        return traits_type::eof();
    }

    int
    sync() override
    {
        flush();
        prepare();
        return 0;
    }

private:
    void
    prepare()
    {
        static std::size_t constexpr max_size = 65536;
        auto const max_prepare = std::min<std::size_t>(
            std::max<std::size_t>(
                512, b_.capacity() - b_.size()),
            std::min<std::size_t>(
                max_size, b_.max_size() - b_.size()));
        auto const bs = b_.prepare(max_prepare);
        auto const b = buffers_front(bs);
        auto const p = static_cast<CharT*>(b.data());
        this->setp(p,
            p + b.size() / sizeof(CharT) - 1);
    }

    void
    flush(int extra = 0)
    {
        b_.commit(
            (this->pptr() - this->pbase() + extra) *
                sizeof(CharT));
    }
};

//------------------------------------------------------------------------------

template<class DynamicBuffer,
    class CharT, class Traits, bool isMovable>
class ostream_helper;

template<class DynamicBuffer, class CharT, class Traits>
class ostream_helper<
        DynamicBuffer, CharT, Traits, true>
    : public std::basic_ostream<CharT, Traits>
{
    ostream_buffer<
        DynamicBuffer, CharT, Traits, true> osb_;

public:
    explicit
    ostream_helper(DynamicBuffer& b);

    ostream_helper(ostream_helper&& other);
};

template<class DynamicBuffer, class CharT, class Traits>
ostream_helper<DynamicBuffer, CharT, Traits, true>::
ostream_helper(DynamicBuffer& b)
    : std::basic_ostream<CharT, Traits>(
        &this->osb_)
    , osb_(b)
{
}

template<class DynamicBuffer, class CharT, class Traits>
ostream_helper<DynamicBuffer, CharT, Traits, true>::
ostream_helper(
        ostream_helper&& other)
    : std::basic_ostream<CharT, Traits>(&osb_)
    , osb_(std::move(other.osb_))
{
}

// This work-around is for libstdc++ versions that
// don't have a movable std::basic_streambuf

template<class T>
class ostream_helper_base
{
protected:
    std::unique_ptr<T> member;

    ostream_helper_base(
        ostream_helper_base&&) = default;

    explicit
    ostream_helper_base(T* t)
        : member(t)
    {
    }
};

template<class DynamicBuffer, class CharT, class Traits>
class ostream_helper<
        DynamicBuffer, CharT, Traits, false>
    : private ostream_helper_base<ostream_buffer<
        DynamicBuffer, CharT, Traits, false>>
    , public std::basic_ostream<CharT, Traits>
{
public:
    explicit
    ostream_helper(DynamicBuffer& b)
        : ostream_helper_base<ostream_buffer<
            DynamicBuffer, CharT, Traits, false>>(
                new ostream_buffer<DynamicBuffer,
                    CharT, Traits, false>(b))
        , std::basic_ostream<CharT, Traits>(this->member.get())
    {
    }

    ostream_helper(ostream_helper&& other)
        : ostream_helper_base<ostream_buffer<
            DynamicBuffer, CharT, Traits, false>>(
                std::move(other))
        , std::basic_ostream<CharT, Traits>(this->member.get())
    {
    }
};

} // detail
} // beast
} // boost

#endif
