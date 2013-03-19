#ifndef __STEM_STEM_HPP
#define __STEM_STEM_HPP

#include <stdexcept>
#include <string>
#include <iostream>

#include <boost/thread/mutex.hpp>

#include "libstemmer.h"

namespace ioremap { namespace wookie {
class stem {
	public:
		stem(const char *lang, const char *enc) {
			m_stem = sb_stemmer_new(lang, enc);
			if (!m_stem)
				throw std::bad_alloc();
		}

		~stem() {
			sb_stemmer_delete(m_stem);
		}

		std::string get(const char *word, int size) {
			const sb_symbol *sb;
			std::string ret;

			boost::mutex::scoped_lock guard(m_lock);

			sb = sb_stemmer_stem(m_stem, (const sb_symbol *)word, size);
			if (!sb)
				return ret;

			int len = sb_stemmer_length(m_stem);

			ret.assign((char *)sb, len);
			return ret;
		}

	private:
		boost::mutex m_lock;
		struct sb_stemmer *m_stem;
};

}}

#endif /* __STEM_STEM_HPP */
