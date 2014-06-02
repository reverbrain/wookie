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

#ifndef __WOOKIE_STORAGE_HPP
#define __WOOKIE_STORAGE_HPP

#include "split.hpp"
#include "index_data.hpp"

#include <elliptics/session.hpp>

#include <fstream>

namespace ioremap { namespace wookie {

class storage {
	public:
		explicit storage(elliptics::node &&node);

		void set_groups(const std::vector<int> groups);
        	void set_namespace(const std::string &ns);

		std::vector<elliptics::find_indexes_result_entry> find(const std::vector<std::string> &indexes);
		std::vector<elliptics::find_indexes_result_entry> find(const std::vector<dnet_raw_id> &indexes);

		elliptics::async_write_result write_document(ioremap::wookie::document &d);
		elliptics::async_read_result read_data(const elliptics::key &key);

		document read_document(const elliptics::key &key);

		static elliptics::data_pointer pack_document(const std::string &url, const std::string &data);
		static elliptics::data_pointer pack_document(ioremap::wookie::document &doc);
		static document unpack_document(const elliptics::data_pointer &result);

		std::vector<dnet_raw_id> transform_tokens(const std::vector<std::string> &tokens);

		elliptics::session create_session(void);
		elliptics::node get_node();

	private:
		elliptics::node m_node;
		elliptics::session m_sess;
		wookie::split m_spl;
};

}}

#endif /* __WOOKIE_STORAGE_HPP */
