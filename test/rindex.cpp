#include <sys/types.h>
#include <sys/syscall.h>

#include <unistd.h>

#include <fstream>
#include <vector>
#include <sstream>
#include <thread>
#include <atomic>

#include <boost/program_options.hpp>

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
			std::cout << "created new downloader object" << std::endl;
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
			std::cout << "async message received" << std::endl;
			aio.loop.unloop();
		}
};

class process {
	public:
		process(int tnum) : m_signal(m_loop), m_downloaders(tnum) {
			m_signal.set<process, &process::signal_received>(this);
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

		void url_complete(const ioremap::swarm::network_reply &reply) {
			std::cout << "url: " << reply.url <<
				", code: " << reply.code <<
				", error: " << reply.error <<
				", data-size: " << reply.data.size() <<
				std::endl;
			for (auto h : reply.headers)
				std::cout << h.first << " : " << h.second << std::endl;
		}


	private:
		ev::default_loop	m_loop;
		ev::sig			m_signal;
		std::vector<wookie::downloader>	m_downloaders;

		void signal_received(ev::sig &sig, int rev) {
			std::cout << "received signal: " << rev << std::endl;
			sig.loop.break_loop();
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

	wookie::process p(3);
	p.feed(url, std::bind(&wookie::process::url_complete, &p, std::placeholders::_1));
	p.start();
}
