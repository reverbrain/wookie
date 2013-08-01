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

#include <thevoid/elliptics/server.hpp>

#include "wookie/storage.hpp"
#include "wookie/split.hpp"

using namespace ioremap;
using namespace ioremap::wookie;

template <typename T>
struct on_upload : public thevoid::elliptics::io::on_upload<T>
{
	std::string m_base_index;
	document m_doc;
	thevoid::elliptics::JsonValue m_result_object;

	/*
	 * this->shared_from_this() returns shared_ptr which contains pointer to the base class without this ugly hack,
	 * thevoid::elliptics::io::on_upload<T> in this case, which in turn doesn't have needed members and declarations
	 *
	 * This hack casts shared pointer to the base class to shared pointer to child class. 
	 */
	std::shared_ptr<on_upload> shared_from_this()
	{
		return std::static_pointer_cast<on_upload>(thevoid::elliptics::io::on_upload<T>::shared_from_this());
	}

	virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) {
		swarm::network_url url(req.get_url());
		swarm::network_query_list query_list(url.query());

		ioremap::elliptics::session sess = this->get_server()->create_session();

		m_base_index = query_list.item_value("base_index");
		m_doc.data.assign(boost::asio::buffer_cast<const char*>(buffer), boost::asio::buffer_size(buffer));
		m_doc.key = query_list.item_value("name");

		sess.write_data(m_doc.key, std::move(storage::pack_document(m_doc)), 0)
				.connect(std::bind(&on_upload<T>::on_write_finished_update_index, this->shared_from_this(),
							std::placeholders::_1, std::placeholders::_2));
	}

	virtual void on_write_finished_update_index(const ioremap::elliptics::sync_write_result &result,
			const ioremap::elliptics::error_info &error) {
		if (error) {
			this->send_reply(swarm::network_reply::service_unavailable);
			return;
		}

		std::vector<std::string> ids;
		std::vector<elliptics::data_pointer> objs;

		// each reverse index contains wookie::index_data object for every key stored
		if (m_doc.data.size()) {
			std::vector<std::string> tokens;
			wookie::mpos_t pos = this->get_server()->get_splitter().feed(m_doc.data, tokens);

			for (auto && p : pos) {
				ids.emplace_back(std::move(p.first));
				objs.emplace_back(wookie::index_data(m_doc.ts, p.first, p.second).convert());
			}
		}

		// base index contains wookie::document object for every key stored
		if (m_base_index.size()) {
			ids.push_back(m_base_index);
			objs.emplace_back(std::move(storage::pack_document(m_doc.key, m_doc.key)));
		}

		if (ids.size()) {
			thevoid::elliptics::io::on_upload<T>::fill_upload_reply(result, m_result_object);

			std::cout << "Rindex update ... url: " << m_doc.key << ": indexes: " << ids.size() << std::endl;
			ioremap::elliptics::session sess = this->get_server()->create_session();
			sess.set_indexes(m_doc.key, ids, objs)
				.connect(std::bind(&on_upload<T>::on_index_update_finished, this->shared_from_this(),
							std::placeholders::_1, std::placeholders::_2));
		} else {
			this->on_write_finished(result, error);
		}
	}

	virtual void on_index_update_finished(const ioremap::elliptics::sync_set_indexes_result &result,
			const ioremap::elliptics::error_info &error) {
		if (error) {
			this->send_reply(swarm::network_reply::service_unavailable);
			return;
		}

		(void) result;

		swarm::network_reply reply;
		reply.set_code(swarm::network_reply::ok);
		reply.set_content_type("text/json");
		reply.set_data(m_result_object.ToString());
		reply.set_content_length(reply.get_data().size());

		this->send_reply(reply);
	}
};

template <typename T>
struct on_get : public thevoid::elliptics::io::on_get<T>
{
	virtual void on_read_finished(const ioremap::elliptics::sync_read_result &result,
			const ioremap::elliptics::error_info &error) {
		if (error.code() == -ENOENT) {
			this->send_reply(swarm::network_reply::not_found);
			return;
		} else if (error) {
			this->send_reply(swarm::network_reply::service_unavailable);
			return;
		}

		const ioremap::elliptics::read_result_entry &entry = result[0];

		document doc = storage::unpack_document(entry.file());

		const swarm::network_request &request = this->get_request();

		if (request.has_if_modified_since()) {
			if ((time_t)doc.ts.tsec <= request.get_if_modified_since()) {
				this->send_reply(swarm::network_reply::not_modified);
				return;
			}
		}

		swarm::network_reply reply;
		reply.set_code(swarm::network_reply::ok);
		reply.set_content_length(doc.data.size());
		reply.set_content_type("text/plain");
		reply.set_last_modified(doc.ts.tsec);

		this->send_reply(reply, std::move(doc.data));
	}
};


template <typename T>
struct on_find : public thevoid::elliptics::index::on_find<T>
{
	virtual void send_indexes_reply(ioremap::elliptics::sync_read_result &data,
			const ioremap::elliptics::sync_find_indexes_result &result) {
		thevoid::elliptics::JsonValue result_object;

		for (size_t i = 0; i < result.size(); ++i) {
			const ioremap::elliptics::find_indexes_result_entry &entry = result[i];

			rapidjson::Value val;
			val.SetObject();

			rapidjson::Value indexes;
			indexes.SetObject();

			for (auto it = entry.indexes.begin(); it != entry.indexes.end(); ++it) {
				const std::string data = it->data.to_string();
				rapidjson::Value value(data.c_str(), data.size(),
						result_object.GetAllocator());
				indexes.AddMember(this->m_map[it->index].c_str(),
						value, result_object.GetAllocator());
			}

			if (data.size()) {
				rapidjson::Value obj;
				obj.SetObject();

				auto it = std::lower_bound(data.begin(), data.end(), entry.id, this->m_rrcmp);
				if (it != data.end()) {
					rapidjson::Value data_str(reinterpret_cast<char *>(it->file().data()),
							it->file().size());
					obj.AddMember("data", data_str, result_object.GetAllocator());

					rapidjson::Value tobj;
					thevoid::elliptics::JsonValue::set_time(tobj,
							result_object.GetAllocator(),
							it->io_attribute()->timestamp.tsec,
							it->io_attribute()->timestamp.tnsec / 1000);
					obj.AddMember("mtime", tobj, result_object.GetAllocator());
				}

				val.AddMember("data-object", obj, result_object.GetAllocator());
			}

			val.AddMember("indexes", indexes, result_object.GetAllocator());

			char id_str[2 * DNET_ID_SIZE + 1];
			dnet_dump_id_len_raw(entry.id.id, DNET_ID_SIZE, id_str);
			result_object.AddMember(id_str, result_object.GetAllocator(),
					val, result_object.GetAllocator());
		}

		swarm::network_reply reply;
		reply.set_code(swarm::network_reply::ok);
		reply.set_content_type("text/json");
		reply.set_data(result_object.ToString());
		reply.set_content_length(reply.get_data().size());

		this->send_reply(reply);
	}
	
};

class http_server :
	public ioremap::thevoid::server<http_server>,
	public ioremap::thevoid::elliptics_server
{
public:
	virtual bool initialize(const rapidjson::Value &config) {
		ioremap::thevoid::elliptics_server::initialize(config);

		on<on_find<http_server>>("/find");
		on<on_get<http_server>>("/get");
		on<on_upload<http_server>>("/upload");
		on<ioremap::thevoid::elliptics::common::on_ping<http_server>>("/ping");
		on<ioremap::thevoid::elliptics::common::on_echo<http_server>>("/echo");
	
		return true;
	}


	wookie::split &get_splitter() {
		return m_splitter;
	}

private:
	wookie::split m_splitter;
};

int main(int argc, char **argv)
{
	return ioremap::thevoid::run_server<http_server>(argc, argv);
}

