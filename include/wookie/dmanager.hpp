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

#ifndef __WOOKIE_DOWNLOAD_HPP
#define __WOOKIE_DOWNLOAD_HPP

#include <swarm/urlfetcher/url_fetcher.hpp>
#include <swarm/urlfetcher/stream.hpp>
#include <swarm/urlfetcher/ev_event_loop.hpp>
#include <swarm/xml/url_finder.hpp>
#include <swarm/url.hpp>
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

#include <wookie/document.hpp>

namespace ioremap { namespace wookie {

class downloader {
	public:
		downloader() :  m_swarm_loop(m_loop), m_async(m_loop), m_manager(m_swarm_loop, m_logger), m_thread(std::bind(&downloader::crawl, this)) {
		}

		~downloader() {
			m_async.send();
			m_thread.join();
		}

		void enqueue(ioremap::swarm::url_fetcher::request &&request,
				const ioremap::swarm::simple_stream::handler_func &handler) {
			m_manager.get(std::make_shared<ioremap::swarm::simple_stream>(handler), std::move(request));
		}

	private:
		swarm::logger m_logger;
		ev::dynamic_loop m_loop;
		swarm::ev_event_loop m_swarm_loop;
		ev::async m_async;
		ioremap::swarm::url_fetcher m_manager;
		std::thread m_thread;

		std::atomic_long m_counter, m_prev_counter;
		std::shared_ptr<ev::async> m_async_exit;
		std::vector<std::shared_ptr<ev::async>> m_async_crawl;

		void crawl() {
			m_async.set<downloader, &downloader::crawl_stop>(this);
			m_async.start();

			m_manager.set_total_limit(10); /* number of active connections */

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

		void feed(const swarm::url &url, const ioremap::swarm::simple_stream::handler_func &handler) {
			ioremap::swarm::url_fetcher::request request;
            request.set_follow_location(true);
            request.set_url(url);
			m_downloaders[rand() % m_downloaders.size()].enqueue(std::move(request), handler);
		}

		void feed(const swarm::url &url, const document &doc, const ioremap::swarm::simple_stream::handler_func &handler) {
			ioremap::swarm::url_fetcher::request request;
            request.set_follow_location(true);
            request.set_url(url);
			request.headers().set_if_modified_since(doc.ts.tsec);

			m_downloaders[rand() % m_downloaders.size()].enqueue(std::move(request), handler);
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
