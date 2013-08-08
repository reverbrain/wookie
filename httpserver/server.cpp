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
#include <thevoid/elliptics/jsonvalue.hpp>

#include "wookie/split.hpp"
#include "wookie/operators.hpp"

using namespace ioremap;
using namespace ioremap::wookie;

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
		set_logger(get_logger_impl());
		m_storage.reset(new storage(get_node().get_log(), ""));

		on<on_find<http_server>>("/find");
		on<on_search>("/search");
		on<ioremap::thevoid::elliptics::io::on_get<http_server>>("/get");
		on<ioremap::thevoid::elliptics::io::on_upload<http_server>>("/upload");
		on<ioremap::thevoid::elliptics::common::on_ping<http_server>>("/ping");
		on<ioremap::thevoid::elliptics::common::on_echo<http_server>>("/echo");
	
		return true;
	}

	ioremap::wookie::storage &get_storage() {
		return *m_storage;
	}

	struct on_search  : public ioremap::thevoid::simple_request_stream<http_server>, public std::enable_shared_from_this<on_search> {
		virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) {
			using namespace std::placeholders;

			(void) buffer;

			ioremap::swarm::network_url url(req.get_url());
			ioremap::swarm::network_query_list query(url.query());

			if (auto text = query.try_item("text")) {
				ioremap::wookie::operators op(get_server()->get_storage());
				op.find(std::bind(&on_search::on_search_finished, shared_from_this(), _1, _2), *text);
			} else {
				send_reply(ioremap::swarm::network_reply::bad_request);
			}
		}

		void on_search_finished(const std::vector<dnet_raw_id> &result, const ioremap::elliptics::error_info &err) {
			if (err) {
				log(ioremap::swarm::LOG_ERROR, "Failed to search: %s", err.message().c_str());
				send_reply(ioremap::swarm::network_reply::service_unavailable);
				return;
			}

			ioremap::thevoid::elliptics::JsonValue result_object;

			rapidjson::Value indexes;
			indexes.SetArray();

			for (size_t i = 0; i < result.size(); ++i) {
				char id_str[2 * DNET_ID_SIZE + 1];
				dnet_dump_id_len_raw(result[i].id, DNET_ID_SIZE, id_str);
				rapidjson::Value index(id_str, result_object.GetAllocator());
				indexes.PushBack(index, result_object.GetAllocator());
			}

			result_object.AddMember("result", indexes, result_object.GetAllocator());

			swarm::network_reply reply;
			reply.set_code(ioremap::swarm::network_reply::ok);
			reply.set_data(result_object.ToString());
			reply.set_content_length(reply.get_data().size());

			send_reply(reply);
		}
	};

private:
	std::unique_ptr<ioremap::wookie::storage> m_storage;
};

int main(int argc, char **argv)
{
	return ioremap::thevoid::run_server<http_server>(argc, argv);
}

