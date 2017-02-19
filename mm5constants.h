#pragma once

#include <type_traits>
#include <cstdint>
#include <utility>

template <class T>
auto &&enum_cast(T &&x) noexcept {
	using U = typename std::underlying_type<T>::type;
	return static_cast<U>(std::forward<T>(x));
}

namespace MM5Sound {



enum class CMD_INDEX : uint8_t {
	
};



}
