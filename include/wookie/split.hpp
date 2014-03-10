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

#ifndef __WOOKIE_SPLIT_HPP
#define __WOOKIE_SPLIT_HPP

#include <map>
#include <string>

#include <boost/locale.hpp>

namespace ioremap { namespace wookie {

typedef std::map<std::string, std::vector<int> > mpos_t;

class split {
	public:
		split() : m_loc(m_gen("en_US.UTF8")) {}

		mpos_t feed(const std::string &text, std::vector<std::string> &tokens) {
			namespace lb = boost::locale::boundary;
			lb::ssegment_index wmap(lb::word, text.begin(), text.end(), m_loc);
			wmap.rule(lb::word_any);

			mpos_t mpos;

			int pos = 0;
			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				std::string token = boost::locale::to_lower(it->str(), m_loc);

				mpos_t::iterator tok = mpos.find(token);
				if (tok != mpos.end()) {
					tok->second.push_back(pos);
				} else {
					std::vector<int> vec;
					vec.push_back(pos);
					mpos.insert(std::make_pair(token, vec));

					tokens.push_back(token);
				}

				++pos;
			}

			return mpos;
		}

	private:
		boost::locale::generator m_gen;
		std::locale m_loc;
};

}}

#endif /* __WOOKIE_SPLIT_HPP */
