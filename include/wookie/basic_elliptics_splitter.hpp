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

#ifndef __WOOKIE_BASIC_ELLIPTICS_SPLITTER_HPP
#define __WOOKIE_BASIC_ELLIPTICS_SPLITTER_HPP

#include <elliptics/utils.hpp>
#include <wookie/document.hpp>
#include <wookie/split.hpp>

#include <atomic>

namespace ioremap { namespace wookie {

// this class splits text into tokens with their positions using @wookie::split
// and then creates array of secondary (reverse) indexes and their index data
// in particular, ids/objs array contains list of reverse indexes where
// @ids contains index IDs (like token names) and @objs array contains set of
// @index_data objects associated with every reverse index
//
// These arrays may also include base index, i.e. some ${prefix}.collection name,
// where all downloaded pages are stored
//
// To date it doesn't perform any lexical processing like text normalization, lemmatization or stemming
// This will be done in WARP project and accessible as a service with async API
class basic_elliptics_splitter {
	public:
		basic_elliptics_splitter() {}
		~basic_elliptics_splitter() {}

		void prepare_indexes(const wookie::document &doc, const std::string &base_index,
				std::vector<std::string> &ids, std::vector<elliptics::data_pointer> &objs);
		void prepare_indexes(const std::string &key, const std::string &content,
				const dnet_time &ts, const std::string &base_index,
				std::vector<std::string> &ids, std::vector<elliptics::data_pointer> &objs);

	private:
		wookie::split m_splitter;
};

}} // namespace ioremap::wookie

#endif /* __WOOKIE_BASIC_ELLIPTICS_SPLITTER_HPP */
