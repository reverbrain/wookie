#ifndef __WOOKIE_DOWNLOAD_HPP
#define __WOOKIE_DOWNLOAD_HPP

#include <swarm/networkmanager.h>
#include <swarm/url_finder.h>
#include <swarm/network_url.h>
#include <elliptics/session.hpp>

#define EV_MULTIPLICITY		1
#define EV_MINIMAL		0
#define EV_USE_MONOTONIC	1
#define EV_USE_REALTIME		1
#define EV_USE_NANOSLEEP	1
#define EV_USE_EVENTFD		1
#include <ev++.h>

#include <atomic>
#include <thread>

namespace ioremap { namespace wookie {

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
			request.set_follow_location(true);
			request.set_url(normalized_url);

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



}}

#endif /* __WOOKIE_DOWNLOAD_HPP */
