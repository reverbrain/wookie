#include "../include/wookie/storage.hpp"

namespace ioremap { namespace wookie {

storage::storage(elliptics::logger &log, const std::string &ns) :
	elliptics::node(log),
	m_namespace(ns) {
}

void storage::set_groups(const std::vector<int> groups) {
	m_groups = groups;
}

std::vector<elliptics::find_indexes_result_entry> storage::find(const std::vector<std::string> &indexes) {
	return std::move(create_session().find_all_indexes(indexes));
}

std::vector<elliptics::find_indexes_result_entry> storage::find(const std::vector<dnet_raw_id> &indexes) {
	return std::move(create_session().find_all_indexes(indexes));
}

elliptics::async_write_result storage::write_document(ioremap::wookie::document &d) {
	msgpack::sbuffer buffer;
	msgpack::pack(&buffer, d);

	return create_session().write_data(d.key, elliptics::data_pointer::copy(buffer.data(), buffer.size()), 0);
}

elliptics::data_pointer storage::pack_document(ioremap::wookie::document &doc) {
	msgpack::sbuffer buffer;
	msgpack::pack(&buffer, doc);

	return elliptics::data_pointer::copy(buffer.data(), buffer.size());
}

elliptics::data_pointer storage::pack_document(const std::string &url, const std::string &data) {
	ioremap::wookie::document doc;
	doc.key = url;
	doc.data = data;
	dnet_current_time(&doc.ts);

	return pack_document(doc);
}

document storage::unpack_document(const elliptics::data_pointer &result) {
	msgpack::unpacked msg;
	msgpack::unpack(&msg, result.data<char>(), result.size());

	document doc;
	msg.get().convert(&doc);

	return doc;
}

elliptics::async_read_result storage::read_data(const elliptics::key &key) {
	return create_session().read_data(key, 0, 0);
}

document storage::read_document(const elliptics::key &key) {
	auto ret = read_data(key);
	ret.wait();

	if (ret.error().code())
		elliptics::throw_error(ret.error().code(), "Could not read url %s", key.to_string().c_str());

	const elliptics::data_pointer &result = ret.get_one().file();
	return unpack_document(result);
}

std::vector<dnet_raw_id> storage::transform_tokens(const std::vector<std::string> &tokens) {
	elliptics::session s(*this);
	std::vector<dnet_raw_id> results;

	results.reserve(tokens.size());
	dnet_raw_id id;

	for (auto t : tokens) {
		s.transform(t, id);
		results.push_back(id);
	}

	return std::move(results);
}

elliptics::session storage::create_session(void) {
	elliptics::session s(*this);

	s.set_groups(m_groups);

	if (m_namespace.size())
		s.set_namespace(m_namespace.c_str(), m_namespace.size());

	s.set_exceptions_policy(elliptics::session::no_exceptions);
	s.set_ioflags(DNET_IO_FLAGS_CACHE);
	s.set_timeout(1000);

	return s;
}

}}
