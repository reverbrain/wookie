#include <sys/types.h>
#include <sys/syscall.h>

#include <unistd.h>

#include <atomic>
#include <cstring>
#include <fstream>
#include <functional>
#include <sstream>
#include <thread>
#include <vector>

#include <boost/program_options.hpp>

#include <libxml/HTMLparser.h>

#define EV_MULTIPLICITY 1
#define EV_MINIMAL       0
#define EV_USE_MONOTONIC 1
#define EV_USE_REALTIME  1
#define EV_USE_NANOSLEEP 1
#define EV_USE_EVENTFD   1
#include <ev++.h>

#include <swarm/networkmanager.h>
#include <swarm/url_finder.h>
#include <swarm/network_url.h>

#include <elliptics/cppdef.h>

#include "wookie/split.hpp"

using namespace ioremap;

namespace ioremap { namespace wookie {

class storage : public elliptics::node {
	public:
		explicit storage(elliptics::logger &log) : elliptics::node(log) {
		}

		void set_groups(const std::vector<int> groups) {
			m_groups = groups;
		}

		void process_file(const std::string &key, const std::string &file) {
			std::ifstream in(file.c_str());
			std::ostringstream ss;
			ss << in.rdbuf();

			process(key, ss.str());
		}

		void process(const std::string &key, const std::string &data) {
			wookie::mpos_t pos = m_spl.feed(data);

			elliptics::session s(*this);
			s.set_groups(m_groups);

			std::vector<std::string> ids;
			for (auto p : pos) {
				ids.push_back(p.first);
			}

			s.set_ioflags(DNET_IO_FLAGS_CACHE | DNET_IO_FLAGS_CACHE_ONLY);
			s.update_indexes(key, ids);
		}

	private:
		std::vector<int> m_groups;
		wookie::split m_spl;
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

			std::cout << "thread started: " << syscall(SYS_gettid) << std::endl;
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

		std::vector<std::string> &tokens(void) {
			return m_tokens;
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

			m_tokens.emplace_back(reinterpret_cast<const char *>(ch), len);
		}

		void parser_end_element(const xmlChar *tag_name) {
			const char *tag = reinterpret_cast<const char*>(tag_name);
			update_process_flag(tag, -1);
		}

	private:
		std::vector<std::string> m_urls;
		std::vector<std::string> m_tokens;
		int m_process_flag;

		void update_process_flag(const char *tag, int offset) {
			static std::string bad_elements[] = {"script", "style"};
			static std::string good_elements[] = {"body"};

			for (auto bad : bad_elements) {
				if (strcasecmp(tag, bad.c_str()) == 0) {
					m_process_flag -= offset;
					return;
				}
			}

			for (auto good : good_elements) {
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
				throw ioremap::elliptics::error(-EINVAL, "Invalid URL '" + url + "': can not be normilized");

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

class recursor {
	public:
		recursor(const std::string &url, const url::recursion recursion, dmanager &d) :
		m_recursion(recursion), m_dmanager(d), m_downloaded(0) {
			ioremap::swarm::network_url base_url;

			if (!base_url.set_base(url))
				throw ioremap::elliptics::error(-EINVAL, "Invalid URL '" + url + "': set-base failed");

			m_base = base_url.host();
			if (m_base.empty())
				throw ioremap::elliptics::error(-EINVAL, "Invalid URL '" + url + "': base is empty");

			m_dmanager.feed(url, std::bind(&recursor::process_reply, this, std::placeholders::_1));
		}

	private:
		std::string m_base;

		url::recursion m_recursion;
		dmanager &m_dmanager;
		std::mutex m_processed_lock;
		std::set<std::string> m_processed;
		std::atomic_long m_downloaded;

		void process_reply(const ioremap::swarm::network_reply &reply) {
			m_downloaded++;

			parser p;
			p.parse(reply.data);

			std::cout << "url: " << reply.url <<
				", code: " << reply.code <<
				", error: " << reply.error <<
				", data-size: " << reply.data.size() <<
				", urls: " << p.urls().size() <<
				", token strings: " << p.tokens().size() <<
				", requested urls: " << m_processed.size() <<
				", downloaded urls: " << m_downloaded <<
				std::endl;
			for (auto h : reply.headers)
				std::cout << h.first << " : " << h.second << std::endl;

			switch (m_recursion) {
			case url::none:
				return;

			case url::full:
			case url::within_domain: {
            			ioremap::swarm::network_url received_url;

				if (!received_url.set_base(reply.url))
					break;

				for (auto url : p.urls()) {
					if (url.compare(0, 2, "//") == 0)
						continue;
					if (url.compare(0, 7, "mailto:") == 0)
						continue;

					std::string host;
					std::string request_url = received_url.relative(url, &host);

					if (request_url.empty() || host.empty())
						continue;

					if ((m_recursion == url::within_domain) && (host != m_base))
						continue;

					{
						std::lock_guard<std::mutex> guard(m_processed_lock);
						if (m_processed.find(request_url) != m_processed.end())
							continue;

						m_processed.insert(request_url);

						m_dmanager.feed(request_url, std::bind(&recursor::process_reply, this, std::placeholders::_1));
					}
				}
				}
			}
		}

};

}} /* namespace ioremap::wookie */

int main(int argc, char *argv[])
{
	namespace po = boost::program_options;
	po::options_description desc("Options (required options are marked with *)");

	int groups_array[] = {1, 2, 3};
	std::vector<int> groups(groups_array, groups_array + ARRAY_SIZE(groups_array));
	std::string log_file;
	int log_level;
	std::string file;
	std::string remote;
	std::string key;
	std::string url;

	desc.add_options()
		("help", "This help message")
		("log-file", po::value<std::string>(&log_file)->default_value("/dev/stdout"), "Log file")
		("log-level", po::value<int>(&log_level)->default_value(DNET_LOG_INFO), "Log level")
		("groups", po::value<std::vector<int> >(&groups), "Groups which will host indexes and data, format: 1:2:3")
		("file", po::value<std::string>(&file)->required(), "Input text file")
		("key", po::value<std::string>(&key), "Which key should be used to store given file into storage")
		("url", po::value<std::string>(&url)->required(), "Url to download")
		("remote", po::value<std::string>(&remote)->required(),
		 	"Remote node to connect, format: address:port:family (IPv4 - 2, IPv6 - 10)")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count("help") || !vm.count("file") || !vm.count("remote")) {
		std::cerr << desc << std::endl;
		return -1;
	}

	if (!vm.count("key"))
		key = file;

	elliptics::file_logger log(log_file.c_str(), log_level);
	wookie::storage st(log);

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
	st.process_file(key, file);

	wookie::dmanager process(3);
	wookie::recursor rec(url, wookie::url::within_domain, process);

	process.start();
}
