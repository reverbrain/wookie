#ifndef __ELLIPTICS_SIMDOC_HPP
#define __ELLIPTICS_SIMDOC_HPP

#include "wookie/tfidf.hpp"
#include "wookie/parser.hpp"

#include "elliptics/session.hpp"

#include <string>
#include <sstream>
#include <vector>

#include <msgpack.hpp>

namespace ioremap { namespace similarity {

struct simdoc {
	enum {
		version = 1,
	};

	int id;
	std::string name;
	std::string text;
	std::vector<wookie::lngram> ngrams;

	wookie::tfidf::tf tf;

	simdoc() : id(0) {}
};

}} // namespace ioremap::similarity

namespace msgpack
{
static inline ioremap::similarity::simdoc &operator >>(msgpack::object o, ioremap::similarity::simdoc &doc)
{
	if (o.type != msgpack::type::ARRAY || o.via.array.size != 4)
		ioremap::elliptics::throw_error(-EPROTO, "msgpack: simdoc type (compiled: %d, unpacked: %d) "
				"or array size mismatch: compiled: %d, unpacked: %d",
				msgpack::type::ARRAY, o.type, 4, o.via.array.size);

	object *p = o.via.array.ptr;

	int version;
	p[0].convert(&version);

	if (version != ioremap::similarity::simdoc::version)
		ioremap::elliptics::throw_error(-EPROTO, "msgpack: simdoc version mismatch: compiled: %d, unpacked: %d",
				ioremap::similarity::simdoc::version, version);

	p[1].convert(&doc.id);
	p[2].convert(&doc.text);
	p[3].convert(&doc.ngrams);

	return doc;
}

template <typename Stream>
static inline msgpack::packer<Stream> &operator <<(msgpack::packer<Stream> &o, const ioremap::similarity::simdoc &d)
{
	o.pack_array(4);
	o.pack(static_cast<int>(ioremap::similarity::simdoc::version));
	o.pack(d.id);
	o.pack(d.text);
	o.pack(d.ngrams);

	return o;
}

} /* namespace msgpack */

#endif /* __ELLIPTICS_SIMDOC_HPP */
