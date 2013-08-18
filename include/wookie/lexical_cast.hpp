#ifndef __WOOKIE_LEXICAL_CAST_HPP
#define __WOOKIE_LEXICAL_CAST_HPP

#include <string>

namespace {
	static inline std::string lexical_cast(size_t value) {
		if (value == 0) {
			return std::string("0");
		}

		std::string result;
		size_t length = 0;
		size_t calculated = value;
		while (calculated) {
			calculated /= 10;
			++length;
		}

		result.resize(length);
		while (value) {
			--length;
			result[length] = '0' + (value % 10);
			value /= 10;
		}

		return result;
	}
}

#endif /* __WOOKIE_LEXICAL_CAST_HPP */
