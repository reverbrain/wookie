#include <sys/types.h>
#include <sys/syscall.h>

#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <boost/program_options.hpp>

#include <libxml/HTMLparser.h>

#define EV_MULTIPLICITY		1
#define EV_MINIMAL		0
#define EV_USE_MONOTONIC	1
#define EV_USE_REALTIME		1
#define EV_USE_NANOSLEEP	1
#define EV_USE_EVENTFD		1
#include <ev++.h>

#include <swarm/networkmanager.h>
#include <swarm/url_finder.h>
#include <swarm/network_url.h>

#include <elliptics/cppdef.h>
#include <elliptics/session_indexes.hpp>

#include "wookie/split.hpp"
#include "wookie/document.hpp"
#include "wookie/index_data.hpp"

using namespace ioremap;

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
			wookie::mpos_t pos = m_spl.feed(data);

			std::vector<std::string> ids;
			std::vector<elliptics::data_pointer> objs;

			for (auto && p : pos) {
				ids.emplace_back(std::move(p.first));
				objs.emplace_back(index_data(ts, p.second).convert());
			}

			create_session().update_indexes(key, ids, objs);
		}

		void find(const std::vector<std::string> &indexes) {
			std::vector<elliptics::find_indexes_result_entry> objs = create_session().find_indexes(indexes);

			for (auto && entry : objs) {
				std::cout << "found document id: " << entry.id << std::endl;
				for (auto index : entry.indexes) {
					index_data id(index.second);
					std::cout << "  token-id: " << index.first << ": " << id << std::endl;
				}
			}
		}

		void find(const std::string &text) {
			wookie::mpos_t pos = m_spl.feed(text);

			std::vector<std::string> indexes;
			std::transform(pos.begin(), pos.end(), std::back_inserter(indexes),
					std::bind(&mpos_t::value_type::first, std::placeholders::_1));

			find(indexes);
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

class downloader {
	public:
		downloader() :  m_async(m_loop), m_manager(m_loop), m_thread(std::bind(&downloader::crawl, this)) {
		}

		~downloader() {
			m_async.send();
			m_thread.join();
		}

		void enqueue(const ioremap::swarm::network_request &request,
				const std::function<void (const ioremap::swarm::network_reply &reply)> &handler) {
			m_manager.get(handler, request);
		}

	private:
		ev::dynamic_loop	m_loop;
		ev::async		m_async;
		ioremap::swarm::network_manager m_manager;
		std::thread		m_thread;

		std::atomic_long m_counter, m_prev_counter;
		std::shared_ptr<ev::async> m_async_exit;
		std::vector<std::shared_ptr<ev::async>> m_async_crawl;

		void crawl() {
			m_async.set<downloader, &downloader::crawl_stop>(this);
			m_async.start();

			m_manager.set_limit(10); /* number of active connections */

			m_loop.loop();
		}

		void crawl_stop(ev::async &aio, int) {
			aio.loop.unloop();
		}
};

class parser {
	public:
		parser() : m_process_flag(0) {}

		void parse(const std::string &page) {
			htmlParserCtxtPtr ctxt;

			htmlSAXHandler handler;
			memset(&handler, 0, sizeof(handler));

			handler.startElement = static_parser_start_element;
			handler.endElement = static_parser_end_element;
			handler.characters = static_parser_characters;

			ctxt = htmlCreatePushParserCtxt(&handler, this, "", 0, "", XML_CHAR_ENCODING_NONE);

			htmlParseChunk(ctxt, page.c_str(), page.size(), 0);
			htmlParseChunk(ctxt, "", 0, 1);

			htmlFreeParserCtxt(ctxt);
		}

		std::vector<std::string> &urls(void) {
			return m_urls;
		}

		const std::string text(void) const {
			return m_text.str();
		}


		void parser_start_element(const xmlChar *tag_name, const xmlChar **attributes) {
			const char *tag = reinterpret_cast<const char*>(tag_name);

			if (strcasecmp(tag, "a") == 0)
				return parse_a(attributes);

			update_process_flag(tag, +1);
		}

		void parser_characters(const xmlChar *ch, int len) {
			if (m_process_flag <= 0)
				return;

			m_text.write(reinterpret_cast<const char *>(ch), len);
		}

		void parser_end_element(const xmlChar *tag_name) {
			const char *tag = reinterpret_cast<const char*>(tag_name);
			update_process_flag(tag, -1);
		}

	private:
		std::vector<std::string> m_urls;
		std::ostringstream m_text;
		int m_process_flag;

		void update_process_flag(const char *tag, int offset) {
			static std::string bad_elements[] = {"script", "style"};
			static std::string good_elements[] = {"body"};

			for (auto && bad : bad_elements) {
				if (strcasecmp(tag, bad.c_str()) == 0) {
					m_process_flag -= offset;
					return;
				}
			}

			for (auto && good : good_elements) {
				if (strcasecmp(tag, good.c_str()) == 0) {
					m_process_flag += offset;
					return;
				}
			}
		}

		static void static_parser_start_element(void *ctx,
						 const xmlChar *tag_name,
						 const xmlChar **attributes) {
			parser *context = reinterpret_cast<parser *>(ctx);
			context->parser_start_element(tag_name, attributes);
		}

		static void static_parser_end_element(void *ctx, const xmlChar *tag_name) {
			parser *context = reinterpret_cast<parser *>(ctx);
			context->parser_end_element(tag_name);
		}

		static void static_parser_characters(void *ctx, const xmlChar *ch, int len) {
			parser *context = reinterpret_cast<parser *>(ctx);
			context->parser_characters(ch, len);
		}

		void parse_a(const xmlChar **attributes) {
			if (!attributes)
				return;

			for (size_t index = 0; attributes[index]; index += 2) {
				const xmlChar *name = attributes[index];
				const xmlChar *value = attributes[index + 1];

				if (!value)
					continue;

				if (strcmp(reinterpret_cast<const char*>(name), "href") == 0) {
					m_urls.push_back(reinterpret_cast<const char*>(value));
				}
			}
		}
};

class dmanager {
	public:
		dmanager(int tnum) : m_signal(m_loop), m_downloaders(tnum) {
			m_signal.set<dmanager, &dmanager::signal_received>(this);
			m_signal.start(SIGTERM);

			srand(time(NULL));
		}

		void start(void) {
			m_loop.loop();
		}

		void feed(const std::string &url, const std::function<void (const ioremap::swarm::network_reply &reply)> &handler) {
			ioremap::swarm::network_url url_parser;
			url_parser.set_base(url);
			std::string normalized_url = url_parser.normalized();
			if (normalized_url.empty())
				ioremap::elliptics::throw_error(-EINVAL, "Invalid URL '%s': URL can not be normilized", url.c_str());

			ioremap::swarm::network_request request;
			request.follow_location = true;
			request.url = normalized_url;

			m_downloaders[rand() % m_downloaders.size()].enqueue(request, handler);
		}

	private:
		ev::default_loop m_loop;
		ev::sig m_signal;
		std::vector<wookie::downloader>	m_downloaders;

		void signal_received(ev::sig &sig, int ) {
			sig.loop.break_loop();
		}
};

namespace url {
	enum recursion {
		none = 1,
		within_domain,
		full
	};
}

}} /* namespace ioremap::wookie */

class url_processor {
	public:
		url_processor(const std::string &url, ioremap::wookie::url::recursion rec,
				ioremap::wookie::storage &st, ioremap::wookie::dmanager &dm) :
		m_recursion(rec),
		m_st(st),
		m_dm(dm),
       		m_total(0) {
			ioremap::swarm::network_url base_url;

			if (!base_url.set_base(url))
				ioremap::elliptics::throw_error(-EINVAL, "Invalid URL '%s': set-base failed", url.c_str());

			m_base = base_url.host();
			if (m_base.empty())
				ioremap::elliptics::throw_error(-EINVAL, "Invalid URL '%s': base is empty", url.c_str());

			download(url);
		}

	private:
		std::string m_base;
		ioremap::wookie::url::recursion m_recursion;

		ioremap::wookie::storage &m_st;
		ioremap::wookie::dmanager &m_dm;

		std::mutex m_inflight_lock;
		std::set<std::string> m_inflight;

		std::atomic_long m_total;

		url_processor(const url_processor &);

		void download(const std::string &url) {
			std::cout << "Downloading ... " << url << std::endl;
			m_dm.feed(url, std::bind(&url_processor::process_url, this, std::placeholders::_1));
		}

		bool inflight_insert(const std::string &url) {
			std::unique_lock<std::mutex> guard(m_inflight_lock);

			auto check = m_inflight.find(url);
			if (check == m_inflight.end()) {
				m_inflight.insert(url);
				return true;
			}

			return false;
		}

		void infligt_erase(const std::string &url) {
			std::unique_lock<std::mutex> guard(m_inflight_lock);
			auto check = m_inflight.find(url);
			if (check != m_inflight.end()) {
				m_inflight.erase(check);
			}
		}

		ioremap::elliptics::async_write_result store_document(const std::string &url, const std::string &content, const dnet_time &ts) {
			infligt_erase(url);

			wookie::document d;
			d.ts = ts;
			d.key = url;
			d.data = content;

			return m_st.write_document(d);
		}

		void process_url(const ioremap::swarm::network_reply &reply) {
			if (reply.error) {
				std::cout << "Error ... " << reply.url << ": " << reply.error << std::endl;
				return;
			}

			std::cout << "Processing  ... " << reply.request.url;
			if (reply.url != reply.request.url)
				std::cout << " -> " << reply.url;

			std::cout << ", total-urls: " << m_total <<
				", data-size: " << reply.data.size() <<
				", headers: " << reply.headers.size() << std::endl;
			for (auto && h : reply.headers)
				std::cout << "  " << h.first << " : " << h.second;

			++m_total;
			std::list<ioremap::elliptics::async_write_result> res;

			struct dnet_time ts;
			dnet_current_time(&ts);

			wookie::parser p;
			p.parse(reply.data);

			try {
				m_st.process(reply.url, p.text(), ts);
			} catch (const std::exception &e) {
				std::cerr << reply.url << ": index processing exception: " << e.what() << std::endl;
				download(reply.request.url);
			}

			res.emplace_back(store_document(reply.url, reply.data, ts));
			if (reply.url != reply.request.url)
				res.emplace_back(store_document(reply.request.url, std::string(), ts));


			if (m_recursion == ioremap::wookie::url::none)
				return;

			ioremap::swarm::network_url received_url;
			if (!received_url.set_base(reply.url))
				ioremap::elliptics::throw_error(-EINVAL, "Could not set network-url-base for orig URL '%s'", reply.url.c_str());

			switch (m_recursion) {
			case ioremap::wookie::url::none: /* can not be here */
			case ioremap::wookie::url::full:
			case ioremap::wookie::url::within_domain:
				for (auto && url : p.urls()) {
					std::string host;
					std::string request_url = received_url.relative(url, &host);

					if ((request_url.compare(0, 6, "https:") != 0) && (request_url.compare(0, 5, "http:") != 0))
						continue;

					if (request_url.empty() || host.empty() || (request_url == reply.url))
						continue;

					if ((m_recursion == ioremap::wookie::url::within_domain) && (host != m_base))
						continue;

					if (!inflight_insert(request_url))
						continue;

					auto rres = m_st.read_data(request_url);
					rres.wait();
					if (rres.error()) {
						download(request_url);
					} else {
						infligt_erase(request_url);
					}
				}
			}

			for (auto && r : res)
				r.wait();
		}
};

int main(int argc, char *argv[])
{
	namespace po = boost::program_options;
	po::options_description desc("Options (required options are marked with *)");

	int groups_array[] = {1, 2, 3};
	std::vector<int> groups(groups_array, groups_array + ARRAY_SIZE(groups_array));
	std::string group_string;
	std::string log_file;
	int log_level;
	std::string remote;
	std::string url;
	std::string ns;
	std::string find;
	int tnum;

	desc.add_options()
		("help", "This help message")
		("log-file", po::value<std::string>(&log_file)->default_value("/dev/stdout"), "Log file")
		("log-level", po::value<int>(&log_level)->default_value(DNET_LOG_INFO), "Log level")
		("groups", po::value<std::string>(&group_string), "Groups which will host indexes and data, format: 1:2:3")
		("url", po::value<std::string>(&url), "Url to download")
		("uthreads", po::value<int>(&tnum)->default_value(3), "Number of URL downloading and processing threads")
		("namespace", po::value<std::string>(&ns), "Namespace for urls and indexes")
		("find", po::value<std::string>(&find), "Find pages containing all tokens (space separated)")
		("remote", po::value<std::string>(&remote)->required(),
		 	"Remote node to connect, format: address:port:family (IPv4 - 2, IPv6 - 10)")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count("help") || !vm.count("remote")) {
		std::cerr << desc << std::endl;
		return -1;
	}

	if (!vm.count("find") && !vm.count("url")) {
		std::cerr << "You must provide either URL or FIND option" << std::endl << desc << std::endl;
		return -1;
	}

	if (group_string.size()) {
		struct digitizer {
			int operator() (const std::string &str) {
				return atoi(str.c_str());
			}
		};

		groups.clear();

		std::istringstream iss(group_string);
		std::transform(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(),
				std::back_inserter<std::vector<int>>(groups), digitizer());
	}

	elliptics::file_logger log(log_file.c_str(), log_level);
	wookie::storage st(log, ns);

	try {
		st.add_remote(remote.c_str());
	} catch (const elliptics::error &e) {
		std::cerr << "Could not connect to " << remote << ": " << e.what() << std::endl;
		return -1;
	}

	boost::locale::generator gen;
	std::locale loc=gen("en_US.UTF8"); 
	std::locale::global(loc);

	st.set_groups(groups);

	try {
		if (find.size()) {
			st.find(find);
		} else {
			wookie::dmanager downloader(tnum);
			url_processor rtest(url, wookie::url::within_domain, st, downloader);
			downloader.start();
		}
	} catch (const std::exception &e) {
		std::cerr << "main thread exception: " << e.what() << std::endl;
	}
	return 0;

}
