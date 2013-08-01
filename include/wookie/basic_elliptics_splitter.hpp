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

#ifndef __WOOKIE_BASIC_ELLIPTICS_SPLITTER_HPP
#define __WOOKIE_BASIC_ELLIPTICS_SPLITTER_HPP

#include <elliptics/utils.hpp>
#include <wookie/document.hpp>
#include <wookie/split.hpp>

namespace ioremap { namespace wookie {

class basic_elliptics_splitter {
	public:
		basic_elliptics_splitter();

		void process(const wookie::document &doc, const std::string &base_index, std::vector<std::string> ids, std::vector<elliptics::data_pointer> objs);
		void process(const std::string &key, const std::string &content, const dnet_time &ts, const std::string &base_index,
				std::vector<std::string> ids, std::vector<elliptics::data_pointer> objs);

	private:
		wookie::split m_splitter;
};

}} // namespace ioremap::wookie

#endif /* __WOOKIE_BASIC_ELLIPTICS_SPLITTER_HPP */
