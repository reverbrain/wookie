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

#include <rift/asio.hpp>
#include <rift/common.hpp>
#include <rift/jsonvalue.hpp>
#include <rift/io.hpp>
#include <rift/server.hpp>
#include <swarm/urlfetcher/url_fetcher.hpp>

#include "wookie/storage.hpp"
#include "wookie/basic_elliptics_splitter.hpp"
#include "wookie/split.hpp"
#include "wookie/operators.hpp"

using namespace ioremap;
using namespace ioremap::wookie;

template <typename T>
struct on_upload : public rift::io::on_upload<T>
{
	std::string m_base_index;
	document m_doc;
	rift::JsonValue m_result_object;

	/*
	 * this->shared_from_this() returns shared_ptr which contains pointer to the base class
	 * without this ugly hack, rift::io::on_upload<T> in this case,
	 * which in turn doesn't have needed members and declarations
	 *
	 * This hack casts shared pointer to the base class to shared pointer to child class. 
	 */

	std::shared_ptr<on_upload> shared_from_this() {
		return std::static_pointer_cast<on_upload>(rift::io::on_upload<T>::shared_from_this());
	}


	virtual void checked(const swarm::http_request &req, const boost::asio::const_buffer &buffer,
			const rift::bucket_meta_raw &meta, swarm::http_response::status_type verdict) {
		if ((verdict != swarm::http_response::ok) && !meta.noauth_all()) {
			this->send_reply(verdict);
			return;
		}

		const swarm::url_query &query_list = req.url().query();

		ioremap::elliptics::session sess = this->server()->elliptics()->session();

		auto base_index = query_list.item_value("base_index");
		auto name = query_list.item_value("name");

		if (!base_index || !name) {
			this->send_reply(ioremap::swarm::http_response::bad_request);
			return;
		}

		m_base_index = *base_index;
		m_doc.data.assign(boost::asio::buffer_cast<const char*>(buffer), boost::asio::buffer_size(buffer));
		m_doc.key = *name;

		sess.write_data(m_doc.key, std::move(storage::pack_document(m_doc)), 0)
				.connect(std::bind(&on_upload<T>::on_write_finished_update_index,
					this->shared_from_this(), std::placeholders::_1, std::placeholders::_2));
	}

	void on_write_finished_update_index(const ioremap::elliptics::sync_write_result &result,
			const ioremap::elliptics::error_info &error) {
		if (error) {
			this->send_reply(swarm::url_fetcher::response::service_unavailable);
			return;
		}

		std::vector<std::string> ids;
		std::vector<elliptics::data_pointer> objs;

		this->server()->get_splitter().prepare_indexes(m_doc, m_base_index, ids, objs);

		if (ids.size()) {
			rift::io::upload_completion::fill_upload_reply(result, m_result_object,
					m_result_object.GetAllocator());

			thevoid::simple_request_stream<T>::log(ioremap::swarm::SWARM_LOG_INFO,
					"rindex update: time: %s, url: '%s', index-number: %zd",
					dnet_print_time(&m_doc.ts), m_doc.key.c_str(), ids.size());
			ioremap::elliptics::session sess = this->server()->elliptics()->session();
			sess.set_indexes(m_doc.key, ids, objs)
				.connect(std::bind(&on_upload<T>::on_index_update_finished,
					this->shared_from_this(), std::placeholders::_1, std::placeholders::_2));
		} else {
			this->on_write_finished(result, error);
		}
	}

	void on_index_update_finished(const ioremap::elliptics::sync_set_indexes_result &result,
			const ioremap::elliptics::error_info &error) {
		if (error) {
			this->send_reply(swarm::url_fetcher::response::service_unavailable);
			return;
		}

		(void) result;

		auto data = m_result_object.ToString();

		swarm::url_fetcher::response reply;
		reply.set_code(swarm::url_fetcher::response::ok);
		reply.headers().set_content_type("text/json");
		reply.headers().set_content_length(data.size());

		this->send_reply(std::move(reply), std::move(data));
	}
};

template <typename T>
struct on_get : public rift::io::on_get<T>
{
	virtual void on_read_finished(const ioremap::elliptics::sync_read_result &result,
			const ioremap::elliptics::error_info &error) {
		if (error.code() == -ENOENT) {
			this->send_reply(swarm::url_fetcher::response::not_found);
			return;
		} else if (error) {
			this->send_reply(swarm::url_fetcher::response::service_unavailable);
			return;
		}

		const ioremap::elliptics::read_result_entry &entry = result[0];

		document doc = storage::unpack_document(entry.file());

		const swarm::http_request &request = this->request();

		if (auto modified_since = request.headers().if_modified_since()) {
			if ((time_t)doc.ts.tsec <= *modified_since) {
				this->send_reply(swarm::url_fetcher::response::not_modified);
				return;
			}
		}

		swarm::url_fetcher::response reply;
		reply.set_code(swarm::url_fetcher::response::ok);
		reply.headers().set_content_length(doc.data.size());
		reply.headers().set_content_type("text/plain");
		reply.headers().set_last_modified(doc.ts.tsec);

		this->send_reply(std::move(reply), std::move(doc.data));
	}
};


class http_server : public ioremap::thevoid::server<http_server>
{
public:
	virtual bool initialize(const rapidjson::Value &config) {
		if (!m_elliptics.initialize(config, logger())) {
			return false;
		}

		m_storage.reset(new storage(elliptics()->session()));

		on<on_get<http_server>>(
			options::exact_match("/get"),
			options::methods("GET")
		);
		on<on_upload<http_server>>(
			options::exact_match("/upload"),
			options::methods("POST")
		);
		on<on_search>(
			options::exact_match("/search"),
			options::methods("GET")
		);
		on<ioremap::rift::common::on_ping<http_server>>(
			options::exact_match("/ping"),
			options::methods("GET")
		);
		on<ioremap::rift::common::on_echo<http_server>>(
			options::exact_match("/echo"),
			options::methods("GET")
		);
	
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
		virtual void on_request(const swarm::http_request &req,
				const boost::asio::const_buffer &buffer) {
			using namespace std::placeholders;

			(void) buffer;

			ioremap::swarm::url url(req.url());
			ioremap::swarm::url_query query(url.query());

			if (auto text = query.item_value("text")) {
				ioremap::wookie::operators op(server()->get_storage());
				shared_find_t fobj = op.find(*text);
			} else {
				send_reply(ioremap::swarm::url_fetcher::response::bad_request);
			}
		}

		void on_search_finished(wookie::find_result &fobj, const ioremap::elliptics::error_info &err) {
			if (err) {
				log(ioremap::swarm::SWARM_LOG_ERROR, "Failed to search: %s", err.message().c_str());
				send_reply(ioremap::swarm::url_fetcher::response::service_unavailable);
				return;
			}

			rift::JsonValue result_object;

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

			auto data = result_object.ToString();

			swarm::url_fetcher::response reply;
			reply.set_code(ioremap::swarm::url_fetcher::response::ok);
			reply.headers().set_content_length(data.size());

			send_reply(std::move(reply), std::move(data));
		}
	};

	const rift::elliptics_base *elliptics() const
	{
		return &m_elliptics;
	}

	void process(const swarm::http_request &request, const boost::asio::const_buffer &buffer,
			const rift::continue_handler_t &continue_handler) const {
		rift::bucket_meta_raw meta;
		continue_handler(request, buffer, meta, swarm::http_response::ok);
	}

	void check_cache(const elliptics::key &, elliptics::session &) const {
	}

	bool query_ok(const swarm::http_request &request) const {
		const auto &query = request.url().query();

		if (auto name = query.item_value("name"))
			return true;

		return false;
	}

	elliptics::session extract_key(const swarm::http_request &request, const rift::bucket_meta_raw &meta,
			elliptics::key &key) const {
		const auto &query = request.url().query();

		auto name = query.item_value("name");
		key = *name;

		elliptics::session session = m_elliptics.session();
		session.set_namespace(meta.key.c_str(), meta.key.size());
		session.set_groups(meta.groups);
		session.transform(key);

		return session;
	}


private:
	wookie::basic_elliptics_splitter m_splitter;
	rift::elliptics_base m_elliptics;

	std::unique_ptr<ioremap::wookie::storage> m_storage;
};

int main(int argc, char **argv)
{
	return ioremap::thevoid::run_server<http_server>(argc, argv);
}

