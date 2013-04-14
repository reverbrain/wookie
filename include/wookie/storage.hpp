#ifndef __WOOKIE_STORAGE_HPP
#define __WOOKIE_STORAGE_HPP

#include "wookie/split.hpp"
#include "wookie/index_data.hpp"

#include <elliptics/cppdef.h>
#include <elliptics/session_indexes.hpp>

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

			process(key, ss.str(), ts);
		}

		void process(const std::string &key, const std::string &data, const dnet_time &ts) {
			std::vector<std::string> tokens;
			wookie::mpos_t pos = m_spl.feed(data, tokens);

			std::vector<std::string> ids;
			std::vector<elliptics::data_pointer> objs;

			for (auto && p : pos) {
				ids.emplace_back(std::move(p.first));
				objs.emplace_back(index_data(ts, p.second).convert());
			}

			create_session().update_indexes(key, ids, objs);
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

			elliptics::session s(*this);
			s.set_exceptions_policy(elliptics::session::exceptions_policy::no_exceptions);
			s.set_groups(m_groups);
			return s.write_data(d.key, elliptics::data_pointer::copy(buffer.data(), buffer.size()), 0);
		}

		elliptics::async_read_result read_data(const std::string &key) {
			elliptics::session s(*this);
			s.set_groups(m_groups);
			s.set_exceptions_policy(elliptics::session::exceptions_policy::no_exceptions);
			return s.read_data(key, 0, 0);
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

	private:
		std::vector<int> m_groups;
		wookie::split m_spl;
		std::string m_namespace;

		elliptics::session create_session(void) {
			elliptics::session s(*this);

			s.set_groups(m_groups);

			if (m_namespace.size())
				s.set_namespace(m_namespace.c_str(), m_namespace.size());

			s.set_ioflags(DNET_IO_FLAGS_CACHE);
			s.set_timeout(10);

			return s;
		}
};

}};

#endif /* __WOOKIE_STORAGE_HPP */
