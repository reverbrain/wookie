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

#ifndef __WOOKIE_TDIDF_HPP
#define __WOOKIE_TDIDF_HPP

#include <map>
#include <string>
#include <vector>

namespace ioremap { namespace wookie { namespace tfidf {

struct word_info {
	std::string word;
	double freq;
};

class tf {
	public:
		tf() : m_total_words(0) {}

		void feed_word(const std::string &word) {
			auto f = m_words.find(word);
			if (f == m_words.end()) {
				m_words[word] = 1;
			} else {
				f->second++;
			}

			++m_total_words;
		}

		double freq(const std::string &word, bool lsmooth) {
			double up = (double)m_words[word];
			if (lsmooth)
				up += 1.0;

			double down = (double)m_total_words;
			if (lsmooth)
				down += (double)m_words.size();

			return up / down;
		}

		size_t word_count(void) const {
			return m_words.size();
		}

		const std::map<std::string, size_t> &words(void) const {
			return m_words;
		}

	private:
		std::map<std::string, size_t> m_words;
		size_t m_total_words;
};

class tfidf {
	public:
		tfidf() : m_need_sort(false) {}

		void feed_word_for_one_file(const std::string &word) {
			m_tf.feed_word(word);

			m_unique.insert(word);
			m_need_sort = true;
		}

		void update_collected_df() {
			for (auto t = m_unique.begin(); t != m_unique.end(); ++t)
				m_df.feed_word(*t);

			m_unique.clear();
		}

		std::vector<word_info> top(size_t num) {
			if (m_need_sort) {
				m_wi.clear();
				m_wi.reserve(m_tf.word_count());

				for (auto it = m_tf.words().begin(); it != m_tf.words().end(); ++it) {
					word_info wi;

					wi.word = it->first;
					wi.freq = it->second / m_df.freq(it->first, true);

					m_wi.emplace_back(wi);
				}

				m_need_sort = false;
			}

			std::vector<word_info> ret(m_wi.begin(), m_wi.begin() + std::min(num, m_wi.size()));
			return ret;
		}

	private:
		wookie::tfidf::tf m_tf;
		wookie::tfidf::tf m_df;
		std::set<std::string> m_unique;

		bool m_need_sort;
		std::vector<word_info> m_wi;
};

}}} // namespace ioremap::wookie::tfidf

#endif /* __WOOKIE_TDIDF_HPP */
