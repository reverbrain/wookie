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

#ifndef __WOOKIE_DOCUMENT_HPP
#define __WOOKIE_DOCUMENT_HPP

#include <time.h>

#include <ostream>

#include <msgpack.hpp>

#include <elliptics/cppdef.h>
#include <elliptics/debug.hpp>

namespace ioremap { namespace wookie {

struct document {
	dnet_time			ts;

	std::string			key;
	std::string			data;

	enum {
		version = 1,
	};

	document() {
		dnet_current_time(&ts);
	}
};

}}

namespace msgpack
{
static inline dnet_time &operator >>(msgpack::object o, dnet_time &tm)
{
	if (o.type != msgpack::type::ARRAY || o.via.array.size != 2)
		throw msgpack::type_error();

	object *p = o.via.array.ptr;
	p[0].convert(&tm.tsec);
	p[1].convert(&tm.tnsec);

	return tm;
}

template <typename Stream>
inline msgpack::packer<Stream> &operator <<(msgpack::packer<Stream> &o, const dnet_time &tm)
{
	o.pack_array(2);
	o.pack(tm.tsec);
	o.pack(tm.tnsec);

	return o;
}

static inline ioremap::wookie::document &operator >>(msgpack::object o, ioremap::wookie::document &d)
{
	if (o.type != msgpack::type::ARRAY || o.via.array.size != 4)
		ioremap::elliptics::throw_error(-EPROTO, "msgpack: document array size mismatch: compiled: %d, unpacked: %d",
				4, o.via.array.size);

	object *p = o.via.array.ptr;

	int version;
	p[0].convert(&version);

	if (version != ioremap::wookie::document::version)
		ioremap::elliptics::throw_error(-EPROTO, "msgpack: document version mismatch: compiled: %d, unpacked: %d",
				ioremap::wookie::document::version, version);

	p[1].convert(&d.ts);
	p[2].convert(&d.key);
	p[3].convert(&d.data);

	return d;
}

template <typename Stream>
static inline msgpack::packer<Stream> &operator <<(msgpack::packer<Stream> &o, const ioremap::wookie::document &d)
{
	o.pack_array(4);
	o.pack(static_cast<int>(ioremap::wookie::document::version));
	o.pack(d.ts);
	o.pack(d.key);
	o.pack(d.data);

	return o;
}

} /* namespace msgpack */

static inline std::ostream &operator <<(std::ostream &out, const ioremap::wookie::document &d)
{
	out << d.ts;
	out << ": key: '" << d.key << "', doc-size: " << d.data.size();
	return out;
}

#endif /* __WOOKIE_DOCUMENT_HPP */
