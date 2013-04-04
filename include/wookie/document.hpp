#include <time.h>

#include <ostream>

#include <msgpack.hpp>

#include "elliptics/cppdef.h"

#ifndef __WOOKIE_DOCUMENT_HPP
#define __WOOKIE_DOCUMENT_HPP

namespace ioremap { namespace wookie {

struct document {
	dnet_time			ts;

	std::string			key;
	std::string			data;
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
	p[0].convert(&tm.tnsec);

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
	if (o.type != msgpack::type::ARRAY || o.via.array.size != 3)
		throw msgpack::type_error();

	object *p = o.via.array.ptr;
	p[0].convert(&d.ts);
	p[1].convert(&d.key);
	p[2].convert(&d.data);

	return d;
}

template <typename Stream>
static inline msgpack::packer<Stream> &operator <<(msgpack::packer<Stream> &o, const ioremap::wookie::document &d)
{
	o.pack_array(3);
	o.pack(d.ts);
	o.pack(d.key);
	o.pack(d.data);

	return o;
}

} /* namespace msgpack */


static inline std::ostream &operator <<(std::ostream &out, const ioremap::wookie::document &d)
{
	char tstr[64];
	struct tm tm;

	localtime_r((time_t *)&d.ts.tsec, &tm);
	strftime(tstr, sizeof(tstr), "%F %R:%S %Z", &tm);

	out.precision(6);
	out << tstr << "." << d.ts.tnsec / 1000 << ": key: '" << d.key << "', doc-size: " << d.data.size();
	return out;
}

#endif /* __WOOKIE_DOCUMENT_HPP */
