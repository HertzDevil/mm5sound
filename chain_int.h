#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>
using std::size_t;

namespace {

// using std::conditional_t;
template <bool B, class T, class F>
using conditional_t = typename std::conditional<B, T, F>::type;

} // namespace

template <size_t N, bool Signed = false>
class ChainInt {
	using elem_type = conditional_t<Signed, int8_t, uint8_t>;
	using max_type = conditional_t<Signed, intmax_t, uintmax_t>;
	static_assert(N > 0, "Cannot chain void integer");
	static_assert(N <= sizeof(max_type), "Chained integer too large");

	elem_type *data_[N];

public:
	template <class... Arg>
	ChainInt(Arg&... args) noexcept : data_ {&args...} { }
	ChainInt(const ChainInt &) = default;
	~ChainInt() noexcept = default;

	operator max_type() const noexcept { return fetch(); }
	elem_type &operator[](size_t n) noexcept { return *data_[n]; }
	const elem_type &operator[](size_t n) const noexcept { return *data_[n]; }

	ChainInt &operator=(const ChainInt &other) noexcept {
		return operator=(static_cast<const max_type&>(other));
	}

	ChainInt &operator=(const max_type &other) noexcept {
		update(other);
		return *this;
	}
#define ASSIGN(FN) \
	ChainInt &operator FN(const max_type &other) noexcept { \
		auto v = fetch(); update(v FN other); \
		return *this; \
	}
	ASSIGN(+=)
	ASSIGN(-=)
	ASSIGN(*=)
	ASSIGN(/=)
	ASSIGN(%=)
	ASSIGN(<<=)
	ASSIGN(>>=)
	ASSIGN(&=)
	ASSIGN(|=)
	ASSIGN(^=)
#undef ASSIGN

#define UNARY(FN) \
	ChainInt &operator FN() noexcept { \
		auto v = fetch(); update(FN v); \
		return *this; \
	} \
	max_type operator FN(int) noexcept { \
		auto v = fetch(); auto out = v FN; update(v); \
		return out; \
	}
	UNARY(++)
	UNARY(--)
#undef UNARY

private:
	max_type fetch() const noexcept {
		max_type z = 0;
		for (size_t i = 0; i < N; ++i) {
			z <<= sizeof(elem_type) * 8;
			z |= static_cast<elem_type>(*data_[i]);
		}
		return z;
	}

	void update(max_type in) const noexcept {
		for (size_t i = 0; i < N; ++i) {
			*data_[N - i - 1] = in;
			in >>= sizeof(elem_type) * 8;
		}
	}
};

namespace {

template <class... Arg>
struct all_lv_refs;
template <class T>
struct all_lv_refs<T> : std::is_lvalue_reference<T> { };
template <class T, class... Arg>
struct all_lv_refs<T, Arg...> : std::integral_constant<bool,
	std::is_lvalue_reference<T>::value && all_lv_refs<Arg...>::value> { };

template <bool B, class T, class... Arg>
struct chain_impl;
template <class T, class... Arg>
struct chain_impl<false, T, Arg...> {
	using U = conditional_t<std::is_signed<T>::value, intmax_t, uintmax_t>;
	U operator()(T &&x, Arg&&... args) const noexcept {
		return combine(x, args...);
	}
private:
	static constexpr U combine() noexcept {
		return 0;
	}
	template <class _T>
	static constexpr U combine(_T x) noexcept {
		return x & static_cast<typename std::decay<T>::type>(~0);
	}
	template <class _T, class... _Arg>
	static constexpr U combine(_T x, _Arg... args) noexcept {
		return (combine(x) << (sizeof...(args) * sizeof(T) * 8))
			| combine(args...);
	}	
};
template <class T, class... Arg>
struct chain_impl<true, T, Arg...> {
	auto operator()(T &&x, Arg&&... args) const noexcept
	-> ChainInt<1 + sizeof...(Arg), std::is_signed<T>::value> {
		return ChainInt<1 + sizeof...(Arg), std::is_signed<T>::value>(x, args...);
	}
};

} // namespace

inline auto chain() noexcept {
	return 0;
}

template <class T, class... Arg>
inline auto chain(T &&x, Arg&&... args) noexcept {
	return chain_impl<all_lv_refs<T, Arg...>::value, T, Arg...>()(
		std::forward<T>(x), std::forward<Arg>(args)...);
}
