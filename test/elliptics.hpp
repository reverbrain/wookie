#ifndef __SIMILARITY_ELLIPTICS_HPP
#define __SIMILARITY_ELLIPTICS_HPP

#include <string>

namespace ioremap { namespace similarity {

static inline std::string elliptics_element_key(const std::string &index) {
	return index + ".elements";
}

}} // namespace ioremap::similarity

#endif /* __SIMILARITY_ELLIPTICS_HPP */
