/*
 * 2013+ Copyright (c) Evgeniy Polyakov <zbr@ioremap.net>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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

void basic_elliptics_splitter::process(const std::string &key, const std::string &content, const dnet_time &ts, const std::string &base_index,
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
		std::cout << std::endl;

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
