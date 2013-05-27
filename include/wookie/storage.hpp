#ifndef __WOOKIE_STORAGE_HPP
#define __WOOKIE_STORAGE_HPP

#include "wookie/split.hpp"
#include "wookie/index_data.hpp"

#include <elliptics/session.hpp>

#include <fstream>

namespace ioremap { namespace wookie {

class storage : public elliptics::node {
	public:
		explicit storage(elliptics::logger &log, const std::string &ns) :
		elliptics::node(log),
       		m_namespace(ns) {
		}

		void set_groups(const std::vector<int> groups) {
			m_groups = groups;
		}

		void process_file(const std::string &key, const std::string &file) {
			std::ifstream in(file.c_str());
			std::ostringstream ss;
			ss << in.rdbuf();

			dnet_time ts;
			dnet_current_time(&ts);

			process(key, ss.str(), ts, std::string());
		}

		void process(const std::string &key, const std::string &data, const dnet_time &ts, const std::string &base_index) {
			std::vector<std::string> ids;
			std::vector<elliptics::data_pointer> objs;

			if (data.size()) {
				std::vector<std::string> tokens;
				wookie::mpos_t pos = m_spl.feed(data, tokens);

				for (auto && p : pos) {
					ids.emplace_back(std::move(p.first));
					objs.emplace_back(index_data(ts, p.first, p.second).convert());
				}
			}

			if (base_index.size()) {
				document doc;
				doc.ts = ts;
				doc.key = key;
				doc.data = key;

				msgpack::sbuffer doc_buffer;
				msgpack::pack(&doc_buffer, doc);

				ids.push_back(base_index);
				objs.emplace_back(elliptics::data_pointer::copy(doc_buffer.data(), doc_buffer.size()));
			}

			if (ids.size()) {
				std::cout << "Updating ... " << key << std::endl;
				create_session().update_indexes(key, ids, objs).wait();
			}
		}

		std::vector<elliptics::find_indexes_result_entry> find(const std::vector<std::string> &indexes) {
			return std::move(create_session().find_indexes(indexes));
		}

		std::vector<elliptics::find_indexes_result_entry> find(const std::vector<dnet_raw_id> &indexes) {
			return std::move(create_session().find_indexes(indexes));
		}

		elliptics::async_write_result write_document(ioremap::wookie::document &d) {
			msgpack::sbuffer buffer;
			msgpack::pack(&buffer, d);

			return create_session().write_data(d.key, elliptics::data_pointer::copy(buffer.data(), buffer.size()), 0);
		}

		elliptics::async_read_result read_data(const elliptics::key &key) {
			return create_session().read_data(key, 0, 0);
		}

		std::vector<dnet_raw_id> transform_tokens(const std::vector<std::string> &tokens) {
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

		elliptics::session create_session(void) {
			elliptics::session s(*this);

			s.set_groups(m_groups);

			if (m_namespace.size())
				s.set_namespace(m_namespace.c_str(), m_namespace.size());

			s.set_exceptions_policy(elliptics::session::no_exceptions);
			s.set_ioflags(DNET_IO_FLAGS_CACHE);
			s.set_timeout(1000);

			return s;
		}

	private:
		std::vector<int> m_groups;
		wookie::split m_spl;
		std::string m_namespace;
};

}};

#endif /* __WOOKIE_STORAGE_HPP */
