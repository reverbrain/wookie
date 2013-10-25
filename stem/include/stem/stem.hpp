/*
 * Copyright 2013+ Evgeniy Polyakov <zbr@ioremap.net>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
			if (!m_stem) {
				m_stem = sb_stemmer_new("eng", enc);
				if (!m_stem)
					throw std::bad_alloc();
			}
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
