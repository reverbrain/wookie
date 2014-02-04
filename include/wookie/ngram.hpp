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

#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace ioremap { namespace wookie {

struct ncount {
	std::string	word;
	int		count;
};

class ngram {
	public:
		static std::map<std::string, int> load_map(const std::string &text, int ngram) {
			std::map<std::string, int> nc;

			if (text.size() >= (size_t)ngram) {
				for (size_t i = 0; i < text.size() - ngram + 1; ++i) {
					std::string word = text.substr(i, ngram);
					auto it = nc.find(word);
					if (it == nc.end())
						nc[word] = 1;
					else
						it->second++;
				}
			}

			return nc;
		}

		static std::vector<ncount> convert(const std::map<std::string, int> &ncmap) {
			std::vector<ncount> ncvec;
			ncvec.reserve(ncmap.size());

			for (auto it = ncmap.begin(); it != ncmap.end(); ++it) {
				ncount nc;
				nc.word = it->first;
				nc.count = it->second;

				ncvec.emplace_back(nc);
			}

			return ncvec;
		}

		static std::vector<ncount> load(const std::string &text, int ngram) {
			std::map<std::string, int> ncmap = load_map(text, ngram);
			return std::move(convert(ncmap));
		}

		static std::map<std::string, int> load_map(const std::vector<std::string> &words, int ngram) {
			std::map<std::string, int> nc;

			for (auto word = words.begin(); word != words.end(); ++word) {
				std::map<std::string, int> tmp_map = load_map(*word, ngram);

				for (auto tmp = tmp_map.begin(); tmp != tmp_map.end(); ++tmp) {
					auto it = nc.find(tmp->first);
					if (it == nc.end())
						nc[tmp->first] = tmp->second;
					else
						it->second += tmp->second;
				}
			}

			return nc;
		}

		static std::vector<ncount> load(const std::vector<std::string> &words, int ngram) {
			std::map<std::string, int> ncmap = load_map(words, ngram);
			return std::move(convert(ncmap));
		}
};

}} // namespace ioremap::wookie

#endif /* __WOOKIE_NGRAM_HPP */
