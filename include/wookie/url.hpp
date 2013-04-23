#ifndef __WOOKIE_URL_HPP
#define __WOOKIE_URL_HPP

#include "wookie/dmanager.hpp"
#include "wookie/parser.hpp"

#include <mutex>

namespace ioremap { namespace wookie {

namespace url {
	enum recursion {
		none = 1,
		within_domain,
		full
	};
}

class url_processor {
	public:
		url_processor(const std::string &url, url::recursion rec,
				storage &st, dmanager &dm) :
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
		url::recursion m_recursion;

		storage &m_st;
		dmanager &m_dm;

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


			if (m_recursion == url::none)
				return;

			ioremap::swarm::network_url received_url;
			if (!received_url.set_base(reply.url))
				ioremap::elliptics::throw_error(-EINVAL, "Could not set network-url-base for orig URL '%s'", reply.url.c_str());

			switch (m_recursion) {
			case url::none: /* can not be here */
			case url::full:
			case url::within_domain:
				for (auto && url : p.urls()) {
					std::string host;
					std::string request_url = received_url.relative(url, &host);

					if ((request_url.compare(0, 6, "https:") != 0) && (request_url.compare(0, 5, "http:") != 0))
						continue;

					if (request_url.empty() || host.empty() || (request_url == reply.url))
						continue;

					if ((m_recursion == url::within_domain) && (host != m_base))
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


}}; /* ioremap::wookie */

#endif /* __WOOKIE_URL_HPP */
