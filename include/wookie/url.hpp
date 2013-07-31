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
