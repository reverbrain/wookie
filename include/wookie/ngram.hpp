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

#ifndef __WOOKIE_NGRAM_HPP
#define __WOOKIE_NGRAM_HPP

#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <vector>

#include <math.h>

namespace ioremap { namespace wookie { namespace ngram {
template <typename S>
struct ncount {
	S	word;
	int	count;

	bool operator<(const struct ncount &other) const {
		return count < other.count;
	}
};

template <typename S, typename D>
class ngram {
	struct ngram_data {
		std::vector<D> data;
		int count;

		ngram_data() : count(1) {}
	};

	public:
		ngram(int n) : m_n(n) {}

		static std::vector<S> split(const S &text, size_t ngram) {
			std::vector<S> ret;

			if (text.size() >= ngram) {
				for (size_t i = 0; i < text.size() - ngram + 1; ++i) {
					S word = text.substr(i, ngram);
					ret.emplace_back(word);
				}
			}

			return ret;
		}

		void load(const S &text, const D &data) {
			std::vector<S> grams = ngram<S, D>::split(text, m_n);
			for (auto word = grams.begin(); word != grams.end(); ++word) {
				auto it = m_map.find(*word);
				if (it == m_map.end()) {
					ngram_data d;
					d.data.push_back(data);
					m_map[*word] = d;
				} else {
					it->second.count++;
					it->second.data.push_back(data);
				}
			}

			//printf("text: %zd bytes, loaded %zd %d-grams\n", text.size(), grams.size(), m_n);
		}

		void convert(void) {
			m_vec.clear();
			m_vec.reserve(m_map.size());

			for (auto it = m_map.begin(); it != m_map.end(); ++it) {
				ncount<S> nc;
				nc.word = it->first;
				nc.count = it->second.count;
				m_vec.emplace_back(nc);

				std::set<D> tmp(it->second.data.begin(), it->second.data.end());
				it->second.data.assign(tmp.begin(), tmp.end());
			}
		}

		std::vector<D> lookup_word(const S &word) const {
			std::vector<D> ret;

			auto it = m_map.find(word);
			if (it != m_map.end())
				ret = it->second.data;

			return ret;
		}

		double lookup(const S &word) const {
			double count = 1.0;

			auto it = m_map.find(word);
			if (it != m_map.end())
				count += it->second.count;

			count /= 2.0 * m_map.size();
			return count;
		}

		size_t num(void) const {
			return m_map.size();
		}

		int n(void) const {
			return m_n;
		}

	private:
		int m_n;
		std::map<S, ngram_data> m_map;
		std::vector<ncount<S>> m_vec;
};

typedef ngram<std::string, int> byte_ngram;

class probability {
	public:
		probability() : m_n2(2), m_n3(3) {}

		bool load_file(const char *filename) {
			std::ifstream in(filename, std::ios::binary);
			std::ostringstream ss;
			ss << in.rdbuf();

			std::string text = ss.str();

			m_n2.load(text, 0);
			m_n3.load(text, 0);
#if 0
			printf("%s: loaded: %zd bytes, 2-grams: %zd, 3-grams: %zd\n",
					filename, text.size(), m_n2.num(), m_n3.num());
#endif
			return true;
		}

		double detect(const std::string &text) const {
			double p = 0;
			for (size_t i = 3; i < text.size(); ++i) {
				std::string s3 = text.substr(i - 3, 3);
				std::string s2 = text.substr(i - 3, 2);

				p += log(m_n3.lookup(s3) / m_n2.lookup(s2));
			}

			return abs(p);
		}

	private:
		byte_ngram m_n2, m_n3;
};

class detector {
	public:
		detector() {}

		bool load_file(const char *filename, const char *id) {
			probability p;
			bool ret = p.load_file(filename);
			if (ret)
				n_prob[id] = p;

			return ret;
		}

		std::string detect(const std::string &text) const {
			double max_p = 0;
			std::string name = "";

			for (auto it = n_prob.begin(); it != n_prob.end(); ++it) {
				double p = it->second.detect(text);
				if (p > max_p) {
					name = it->first;
					max_p = p;
				}
			}

			return name;
		}

		std::string detect_file(const char *filename) const {
			std::ifstream in(filename, std::ios::binary);
			std::ostringstream ss;
			ss << in.rdbuf();

			std::string text = ss.str();
			return detect(text);
		}

	private:
		std::map<std::string, probability> n_prob;
};

}}} // namespace ioremap::wookie::ngram

#endif /* __WOOKIE_NGRAM_HPP */
