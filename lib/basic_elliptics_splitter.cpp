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

#include <wookie/basic_elliptics_splitter.hpp>
#include <wookie/storage.hpp>

using namespace ioremap;
using namespace ioremap::wookie;

basic_elliptics_splitter::~basic_elliptics_splitter()
{
	for (const auto &t: m_tokens) {
		std::cout << t.first << ": " << t.second << std::endl;
	}
}

void basic_elliptics_splitter::process(const std::string &key, const std::string &content,
		const dnet_time &ts, const std::string &base_index,
		std::vector<std::string> &ids, std::vector<elliptics::data_pointer> &objs)
{
	// each reverse index contains wookie::index_data object for every key stored
	if (content.size()) {
		std::vector<std::string> tokens;
		wookie::mpos_t pos = m_splitter.feed(content, tokens);

		std::cout << "split: key: " << key << ", tokens: " << tokens.size() << ", positions: " << pos.size() << std::endl;
		for (auto &t : tokens) {
			(m_tokens[t])++;
		}

		for (auto && p : pos) {
			ids.emplace_back(std::move(p.first));
			objs.emplace_back(wookie::index_data(ts, p.first, p.second).convert());
		}
	}

	// base index contains wookie::document object for every key stored
	if (base_index.size()) {
		wookie::document tmp;
		tmp.ts = ts;
		tmp.key = key;
		tmp.data = key;

		ids.push_back(base_index);
		objs.emplace_back(std::move(storage::pack_document(tmp)));
	}
}

void basic_elliptics_splitter::process(const wookie::document &doc, const std::string &base_index, std::vector<std::string> &ids, std::vector<elliptics::data_pointer> &objs)
{
	process(doc.key, doc.data, doc.ts, base_index, ids, objs);
}
