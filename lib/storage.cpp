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

#include "wookie/storage.hpp"

namespace ioremap { namespace wookie {

storage::storage(const elliptics::session &sess) : m_sess(sess.clone()) {
	m_sess.set_exceptions_policy(elliptics::session::no_exceptions);
	m_sess.set_ioflags(DNET_IO_FLAGS_CACHE);
	m_sess.set_timeout(1000);
}

void storage::set_groups(const std::vector<int> groups) {
	m_sess.set_groups(groups);
}

void storage::set_namespace(const std::string &ns) {
	m_sess.set_namespace(ns.c_str(), ns.size());
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
	elliptics::session s = create_session();
	std::vector<dnet_raw_id> results;

	results.reserve(tokens.size());
	dnet_raw_id id;

	for (auto t : tokens) {
		s.transform(t, id);
		results.emplace_back(id);
	}

	return std::move(results);
}

elliptics::session storage::create_session(void) {
	return m_sess.clone();
}

elliptics::node storage::get_node()
{
	return m_sess.get_node();
}

}}
