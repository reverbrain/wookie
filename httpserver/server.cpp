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

#include "wookie/split.hpp"

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

		on<on_find<http_server>>("/find");
		on<ioremap::thevoid::elliptics::io::on_get<http_server>>("/get");
		on<ioremap::thevoid::elliptics::io::on_upload<http_server>>("/upload");
		on<ioremap::thevoid::elliptics::common::on_ping<http_server>>("/ping");
		on<ioremap::thevoid::elliptics::common::on_echo<http_server>>("/echo");
	
		return true;
	}
};

int main(int argc, char **argv)
{
	return ioremap::thevoid::run_server<http_server>(argc, argv);
}

