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

#ifndef __WOOKIE_URL_HPP
#define __WOOKIE_URL_HPP

#include "dmanager.hpp"
#include "parser.hpp"
#include "storage.hpp"

#include <mutex>

#include <magic.h>

namespace ioremap { namespace wookie {

namespace url {
	enum recursion {
		none = 1,
		within_domain,
		full
	};
}

class magic {
	public:
		magic() {
			m_magic = magic_open(MAGIC_MIME);
			if (!m_magic)
				ioremap::elliptics::throw_error(-ENOMEM, "Failed to create MIME magic handler");

			if (magic_load(m_magic, 0) == -1) {
				magic_close(m_magic);
				ioremap::elliptics::throw_error(-ENOMEM, "Failed to load MIME magic database");
			}
		}

		~magic() {
			magic_close(m_magic);
		}

		const char *type(const char *buffer, size_t size) {
			const char *ret = magic_buffer(m_magic, buffer, size);

			if (!ret)
				ret = "none";

			return ret;
		}

		bool is_text(const char *buffer, size_t size) {
			return !strncmp(type(buffer, size), "text/", 5);
		}

	private:
		magic_t m_magic;
};

}}; /* ioremap::wookie */

#endif /* __WOOKIE_URL_HPP */
