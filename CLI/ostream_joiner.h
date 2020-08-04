#pragma once

// This file allows me to use std::experimental::ostream_joiner without waiting for Microsoft to create it.
// Reference: http://en.cppreference.com/w/cpp/experimental/ostream_joiner

#if !defined(HAS_OSTREAM_JOINER_H)
#  if defined(__has_include)
#    if __has_include(<experimental/iterator>)
#      define HAS_OSTREAM_JOINER_H
#    endif /* __has_include(<experimental/iterator>) */
#  endif   /* __has_include */
#endif     /* HAS_OSTREAM_JOINER_H */

#if !defined(HAS_OSTREAM_JOINER_H)
#  include <ostream>

namespace std::experimental {
template <class DelimT, class CharT = char, class Traits = char_traits<CharT>> class ostream_joiner {
public:
  using char_type    = CharT;
  using traits_type  = Traits;
  using ostream_type = basic_ostream<char_type, traits_type>;

  using value_type        = void;
  using difference_type   = void;
  using pointer           = void;
  using reference         = void;
  using iterator_category = output_iterator_tag;

  // ReSharper disable once CppRedundantAccessSpecifier
public:
  ostream_joiner(ostream_type &stream, const DelimT &delimiter)
      : m_bFirst(true), m_pStream(addressof(stream)), m_Delimiter(delimiter) {}
  ostream_joiner(ostream_type &stream, DelimT &&delimiter)
      : m_bFirst(true), m_pStream(addressof(stream)), m_Delimiter(forward<DelimT>(delimiter)) {}
  ostream_joiner(const ostream_joiner &other) = default;
  ostream_joiner(ostream_joiner &&other)      = default;
  ~ostream_joiner()                           = default;

  // ReSharper disable once CppRedundantAccessSpecifier
public:
  template <class T> ostream_joiner &operator=(const T &value) {
    if (!m_bFirst) *m_pStream << m_Delimiter;

    m_bFirst = false;
    *m_pStream << value;

    return *this;
  }
  ostream_joiner &operator=(const ostream_joiner &other) = default;
  ostream_joiner &operator=(ostream_joiner &&other) = default;

  // ReSharper disable once CppRedundantAccessSpecifier
public:
  ostream_joiner &operator*() noexcept { return *this; }
  ostream_joiner &operator++() noexcept { return *this; }
  ostream_joiner &operator++(int) noexcept { return *this; }

private:
  bool m_bFirst;
  ostream_type *m_pStream;
  DelimT m_Delimiter;
};

// ReSharper disable once CppRedundantInlineSpecifier
template <class charT, class traits, class DelimT>
ostream_joiner<decay_t<DelimT>, charT, traits> static inline make_ostream_joiner(
    basic_ostream<charT, traits> &os, DelimT &&delimiter) {
  return ostream_joiner<decay_t<DelimT>, charT, traits>(os, forward<DelimT>(delimiter));
}
} // namespace std::experimental
#else
#  include <experimental/iterator>
#endif