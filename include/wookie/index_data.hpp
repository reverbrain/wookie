#ifndef __INDEX_DATA_HPP
#define __INDEX_DATA_HPP

#include "wookie/document.hpp"

#include "elliptics/session.hpp"
#include "elliptics/debug.hpp"

#include "elliptics/packet.h"

#include <msgpack.hpp>

namespace ioremap { namespace wookie {

struct index_data {
	dnet_time ts;
	std::string key;
	std::vector<int> pos;

	index_data(const dnet_time &new_ts, const std::string &new_key, std::vector<int> &new_pos) :
	ts(new_ts),
	key(new_key),
	pos(new_pos) {
	}

	index_data(const elliptics::data_pointer &d) {
		msgpack::unpacked msg;
		msgpack::unpack(&msg, d.data<char>(), d.size());
		msg.get().convert(this);
	}

	elliptics::data_pointer convert() {
		msgpack::sbuffer buffer;
		msgpack::pack(&buffer, *this);

		return elliptics::data_pointer::copy(buffer.data(), buffer.size());
	}

	enum {
		version = 2,
	};
};

}}; /* namespace ioremap::wookie */

inline std::ostream &operator <<(std::ostream &out, const ioremap::wookie::index_data &id)
{
	out << id.ts << ", positions in document: ";
	for (auto p : id.pos)
		out << p << " ";

	return out;
}

namespace msgpack {
static inline ioremap::wookie::index_data &operator >>(msgpack::object o, ioremap::wookie::index_data &d)
{
	if (o.type != msgpack::type::ARRAY || o.via.array.size != 4)
		ioremap::elliptics::throw_error(-EPROTO, "msgpack: index data array size mismatch: compiled: %d, unpacked: %d",
				3, o.via.array.size);

	object *p = o.via.array.ptr;

	int version;
	p[0].convert(&version);

	if (version != ioremap::wookie::index_data::version)
		ioremap::elliptics::throw_error(-EPROTO, "msgpack: index data version mismatch: compiled: %d, unpacked: %d",
				ioremap::wookie::index_data::version, version);

	p[1].convert(&d.ts);
	p[2].convert(&d.pos);
	p[3].convert(&d.key);

	return d;
}

template <typename Stream>
inline msgpack::packer<Stream> &operator <<(msgpack::packer<Stream> &o, const ioremap::wookie::index_data &d)
{
	o.pack_array(4);
	o.pack(static_cast<int>(ioremap::wookie::index_data::version));
	o.pack(d.ts);
	o.pack(d.pos);
	o.pack(d.key);

	return o;
}

}; /* namespace msgpack */

#endif /* __INDEX_DATA_HPP */
