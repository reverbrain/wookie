#ifndef __STEM_STEM_HPP
#define __STEM_STEM_HPP

#include <stdio.h>

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

		std::string get(const char *word, int size, bool complete = true) {
			const sb_symbol *sb;
			std::string ret;

			int len, prev_len;
			len = prev_len = size;

			boost::mutex::scoped_lock guard(m_lock);

			do {
				sb = sb_stemmer_stem(m_stem, (const sb_symbol *)word, len);
				if (!sb)
					return ret;

				len = sb_stemmer_length(m_stem);
				if (len == prev_len)
					break;

				prev_len = len;
			} while (complete);

			ret.assign((char *)sb, len);
			return ret;
		}

	private:
		boost::mutex m_lock;
		struct sb_stemmer *m_stem;
};

}}

#endif /* __STEM_STEM_HPP */
