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

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ioremap { namespace wookie { namespace tfidf {

struct word_info {
	std::string word;
	double freq;

	// sort in descending order
	bool operator ()(const word_info &f, const word_info &s) {
		return f.freq >= s.freq;
	}
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

		size_t count(const std::string &word) {
			size_t num = 0;
			auto it = m_words.find(word);
			if (it != m_words.end())
				num = it->second;

			return num;
		}

		double freq(const std::string &word, bool lsmooth) {
			double up = (double)count(word);
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
		tfidf() {}

		void feed_word_for_one_file(const std::string &word) {
			m_unique.insert(word);
		}

		void update_collected_df() {
			for (auto t = m_unique.begin(); t != m_unique.end(); ++t)
				m_df.feed_word(*t);

			m_unique.clear();
		}

		std::vector<word_info> top(const tf &tf, size_t num) {
			std::vector<word_info> wis;
			wis.reserve(tf.word_count());

			for (auto it = tf.words().begin(); it != tf.words().end(); ++it) {
				word_info wi;

				wi.word = it->first;
				wi.freq = (double)it->second / m_df.count(it->first);

				wis.emplace_back(wi);
			}

			std::sort(wis.begin(), wis.end(), word_info());
			wis.resize(std::min(num, wis.size()));
			return wis;
		}

	private:
		wookie::tfidf::tf m_df;
		std::set<std::string> m_unique;
};

}}} // namespace ioremap::wookie::tfidf

#endif /* __WOOKIE_TDIDF_HPP */
