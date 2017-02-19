#pragma once

#include <cstdint>
#include <type_traits>

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
	max_type x_;

public:
	template <class... Arg>
	ChainInt(Arg&... args) noexcept : data_ {&args...}, x_(combine(args...)) { }
	~ChainInt() noexcept = default;

	operator max_type() const noexcept { return x_; }
	elem_type &operator[](size_t n) noexcept { return *data_[n]; }
	const elem_type &operator[](size_t n) const noexcept { return *data_[n]; }

	ChainInt &operator=(const ChainInt &other) noexcept {
		return operator=(static_cast<const max_type&>(other));
	}

#define ASSIGN(FN) \
	ChainInt &operator FN(const max_type &other) noexcept { \
		x_ FN other; update(); return *this; }
	ASSIGN(=)
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
	ChainInt &operator FN() noexcept { FN x_; update(); return *this; } \
	max_type operator FN(int) noexcept { auto v = x_; operator FN(); return v; }
	UNARY(++)
	UNARY(--)
#undef UNARY

private:
	void update() const noexcept {
		auto v = x_;
		for (size_t i = 0; i < N; ++i) {
			*data_[N - i - 1] = v;
			v >>= sizeof(elem_type) * 8;
		}
	}

	static constexpr max_type combine() noexcept {
		return 0;
	}
	static constexpr max_type combine(elem_type x) noexcept {
		return x & ((elem_type)-1);
	}
	template <class... Arg>
	static constexpr max_type combine(elem_type x, Arg... args) noexcept {
		return (combine(x) << (sizeof...(args) * sizeof(elem_type) * 8))
			| combine(args...);
	}
};

template <class T, class... Arg>
auto chain(T &x, Arg&... args) noexcept {
	return ChainInt<1 + sizeof...(Arg), std::is_signed<T>::value>(x, args...);
}
