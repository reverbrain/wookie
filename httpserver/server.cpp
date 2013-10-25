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

#include <thevoid/elliptics/jsonvalue.hpp>
#include <thevoid/elliptics/server.hpp>

#include "wookie/storage.hpp"
#include "wookie/basic_elliptics_splitter.hpp"
#include "wookie/split.hpp"
#include "wookie/operators.hpp"

using namespace ioremap;
using namespace ioremap::wookie;

template <typename T>
struct on_upload : public thevoid::elliptics::io::on_upload<T>
{
	std::string m_base_index;
	document m_doc;
	thevoid::elliptics::JsonValue m_result_object;

	/*
	 * this->shared_from_this() returns shared_ptr which contains pointer to the base class
	 * without this ugly hack, thevoid::elliptics::io::on_upload<T> in this case,
	 * which in turn doesn't have needed members and declarations
	 *
	 * This hack casts shared pointer to the base class to shared pointer to child class. 
	 */
	std::shared_ptr<on_upload> shared_from_this()
	{
		return std::static_pointer_cast<on_upload>(
				thevoid::elliptics::io::on_upload<T>::shared_from_this());
	}

	virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) {
		swarm::network_url url(req.get_url());
		swarm::network_query_list query_list(url.query());

		ioremap::elliptics::session sess = this->get_server()->create_session();

		m_base_index = query_list.item_value("base_index");
		m_doc.data.assign(boost::asio::buffer_cast<const char*>(buffer), boost::asio::buffer_size(buffer));
		m_doc.key = query_list.item_value("name");

		sess.write_data(m_doc.key, std::move(storage::pack_document(m_doc)), 0)
				.connect(std::bind(&on_upload<T>::on_write_finished_update_index,
					this->shared_from_this(), std::placeholders::_1, std::placeholders::_2));
	}

	virtual void on_write_finished_update_index(const ioremap::elliptics::sync_write_result &result,
			const ioremap::elliptics::error_info &error) {
		if (error) {
			this->send_reply(swarm::network_reply::service_unavailable);
			return;
		}

		std::vector<std::string> ids;
		std::vector<elliptics::data_pointer> objs;

		this->get_server()->get_splitter().process(m_doc, m_base_index, ids, objs);

		if (ids.size()) {
			thevoid::elliptics::io::on_upload<T>::fill_upload_reply(result, m_result_object);

			thevoid::simple_request_stream<T>::log(ioremap::swarm::LOG_INFO,
					"rindex update: time: %s, url: '%s', index-number: %zd",
					dnet_print_time(&m_doc.ts), m_doc.key.c_str(), ids.size());
			ioremap::elliptics::session sess = this->get_server()->create_session();
			sess.set_indexes(m_doc.key, ids, objs)
				.connect(std::bind(&on_upload<T>::on_index_update_finished,
					this->shared_from_this(), std::placeholders::_1, std::placeholders::_2));
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


class http_server :
	public ioremap::thevoid::server<http_server>,
	public ioremap::thevoid::elliptics_server
{
public:
	virtual bool initialize(const rapidjson::Value &config) {
		ioremap::thevoid::elliptics_server::initialize(config);
		set_logger(get_logger_impl());
		m_storage.reset(new storage(create_session()));

		on<on_get<http_server>>("/get");
		on<on_upload<http_server>>("/upload");
		on<on_search>("/search");
		on<ioremap::thevoid::elliptics::common::on_ping<http_server>>("/ping");
		on<ioremap::thevoid::elliptics::common::on_echo<http_server>>("/echo");
	
		return true;
	}

	wookie::basic_elliptics_splitter &get_splitter() {
		return m_splitter;
	}

	ioremap::wookie::storage &get_storage() {
		return *m_storage;
	}

	struct on_search  : public ioremap::thevoid::simple_request_stream<http_server>,
			    public std::enable_shared_from_this<on_search> {
		virtual void on_request(const swarm::network_request &req,
				const boost::asio::const_buffer &buffer) {
			using namespace std::placeholders;

			(void) buffer;

			ioremap::swarm::network_url url(req.get_url());
			ioremap::swarm::network_query_list query(url.query());

			if (auto text = query.try_item("text")) {
				ioremap::wookie::operators op(get_server()->get_storage());
				shared_find_t fobj = op.find(*text);
			} else {
				send_reply(ioremap::swarm::network_reply::bad_request);
			}
		}

		void on_search_finished(wookie::find_result &fobj, const ioremap::elliptics::error_info &err) {
			if (err) {
				log(ioremap::swarm::LOG_ERROR, "Failed to search: %s", err.message().c_str());
				send_reply(ioremap::swarm::network_reply::service_unavailable);
				return;
			}

			ioremap::thevoid::elliptics::JsonValue result_object;

			rapidjson::Value indexes;
			indexes.SetArray();

			auto ids = fobj.results_array();
			for (size_t i = 0; i < ids.size(); ++i) {
				char id_str[2 * DNET_ID_SIZE + 1];
				dnet_dump_id_len_raw(ids[i].id, DNET_ID_SIZE, id_str);
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
	wookie::basic_elliptics_splitter m_splitter;

	std::unique_ptr<ioremap::wookie::storage> m_storage;
};

int main(int argc, char **argv)
{
	return ioremap::thevoid::run_server<http_server>(argc, argv);
}

