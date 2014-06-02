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

#include "wookie/engine.hpp"
#include "wookie/storage.hpp"
#include "wookie/dmanager.hpp"
#include "wookie/parser.hpp"
#include "wookie/lexical_cast.hpp"
#include "wookie/url.hpp"

#include <mutex>

#include <boost/algorithm/string.hpp>

namespace ioremap { namespace wookie {

filter_functor create_text_filter()
{
	struct filter
	{
		wookie::magic magic;

		bool check(const swarm::url_fetcher::response &reply, const std::string &data)
		{
			if (auto content_type = reply.headers().content_type()) {
				std::cout << *content_type << std::endl;

				return content_type->compare(0, 5, "text/", 5) == 0;
			} else {
				return magic.is_text(data.c_str(), data.size());
			}
		}
	};

	return std::bind(&filter::check, std::make_shared<filter>(), std::placeholders::_1, std::placeholders::_2);
}

url_filter_functor create_domain_filter(const std::string &url)
{
	struct filter
	{
		std::string base_host;

		filter(const ioremap::swarm::url &url)
		{
			base_host = url.host();
			if (base_host.empty())
				ioremap::elliptics::throw_error(-EINVAL, "Invalid URL '%s': base is empty", url.to_string().c_str());
		}

		bool check(const swarm::url_fetcher::response &reply, const swarm::url &url)
		{
			return base_host == reply.url().resolved(url).host();
		}
	};
	return std::bind(&filter::check, std::make_shared<filter>(url), std::placeholders::_1, std::placeholders::_2);
}

url_filter_functor create_port_filter(const std::vector<int> &ports)
{
	struct filter
	{
		std::vector<int> allowed_ports;

		filter(const std::vector<int> &ports) : allowed_ports(ports) {
		}

		bool check(const swarm::url &url) {
			if (auto port = url.port()) {
				auto it = std::find(allowed_ports.begin(), allowed_ports.end(), *port);
				return it != allowed_ports.end();
			}

			return true;
		}
	};

	return std::bind(&filter::check, std::make_shared<filter>(ports), std::placeholders::_2);
}

parser_functor create_href_parser()
{
	struct parser
	{
		std::vector<std::string> operator() (const swarm::url_fetcher::response &reply, const std::string &data)
		{
			(void) reply;

			wookie::parser p;
			p.feed_text(data);

			return p.urls();
		}
	};

	return parser();
}

class engine_data
{
public:
	std::vector<boost::program_options::options_description> options;
	std::vector<parser_functor> parsers;
	std::vector<filter_functor> filters;
	std::vector<url_filter_functor> url_filters;
	std::vector<process_functor> processors;
	std::vector<process_functor> fallback_processors;
	std::unique_ptr<wookie::storage> storage;
	std::unique_ptr<wookie::dmanager> downloader;
	boost::program_options::options_description command_line_options;

	std::mutex inflight_lock;
	std::map<std::string, document> inflight;

	std::atomic_long total;
	wookie::magic magic;

	struct dnet_time generation_time;

	engine_data() : total(0) {
		dnet_current_time(&generation_time);
	}

	void download(const swarm::url &url) {
		std::cout << "Downloading ... " << url.to_string() << std::endl;
		using namespace std::placeholders;
		downloader->feed(url, std::bind(&engine_data::process_url, this, _1, _2, _3));
	}

	void found_in_page_cache(const swarm::url &url, const document &doc) {
		std::cout << "Downloading (if-modified-since " << doc.ts << ") ... " << url.to_string() << std::endl;
		inflight_insert(url, doc);
		using namespace std::placeholders;
		downloader->feed(url, doc, std::bind(&engine_data::process_url, this, _1, _2, _3));
	}

	bool inflight_insert(const swarm::url &url, const document &doc) {
		const std::string url_string = url.to_string();

		std::unique_lock<std::mutex> guard(inflight_lock);
		return inflight.insert(std::make_pair(url_string, doc)).second;
	}

	bool inflight_insert(const swarm::url &url) {
		return inflight_insert(url, document());
	}

	document inflight_erase(const swarm::url &url) {
		document d;

		const std::string url_string = url.to_string();

		std::unique_lock<std::mutex> guard(inflight_lock);
		auto check = inflight.find(url_string);
		if (check != inflight.end()) {
			d = check->second;
			inflight.erase(check);
		}

		return d;
	}

	ioremap::elliptics::async_write_result store_document(const swarm::url &url, const std::string &content, const dnet_time &ts) {
		wookie::document d;
		d.ts = ts;
		d.key = url.to_string();
		d.data = content;

		return storage->write_document(d);
	}

	void process_reply(const swarm::url_fetcher::response &reply, const std::string &data) {
		std::cout << "Processing  ... " << reply.request().url().to_string();
		if (reply.url().to_string() != reply.request().url().to_string())
			std::cout << " -> " << reply.url().to_string();

		std::cout << ", code: " << reply.code() <<
			     ", total-urls: " << total <<
			     ", data-size: " << data.size() <<
			     ", headers: " << reply.headers().all().size() <<
			     std::endl;

		bool accepted_by_filters = true;
		for (auto it = filters.begin(); accepted_by_filters && it != filters.end(); ++it) {
			accepted_by_filters &= (*it)(reply, data);
		}

		++total;
		std::list<ioremap::elliptics::async_write_result> res;

		struct dnet_time ts;
		dnet_current_time(&ts);

		res.emplace_back(store_document(reply.url(), data, ts));

		// if original URL redirected to other location, store object by original URL too
		if (reply.url().to_string() != reply.request().url().to_string())
			res.emplace_back(store_document(reply.request().url(), data, ts));

		if (accepted_by_filters) {
			if (reply.code() != ioremap::swarm::url_fetcher::response::not_modified) {
				for (auto it = processors.begin(); it != processors.end(); ++it)
					(*it)(reply, data, document_new);
			}

			std::vector<std::string> urls;

			for (auto it = parsers.begin(); it != parsers.end(); ++it) {
				const auto new_urls = (*it)(reply, data);
				urls.insert(urls.end(), new_urls.begin(), new_urls.end());
			}

			std::sort(urls.begin(), urls.end());
			urls.erase(std::unique(urls.begin(), urls.end()), urls.end());

			const swarm::url &base_url = reply.url();

			for (auto it = urls.begin(); it != urls.end(); ++it) {
				swarm::url relative_url = *it;
				if (!relative_url.is_valid()) {
					continue;
				}

				swarm::url request_url = base_url.resolved(relative_url);
				if (!request_url.is_valid()) {
					continue;
				}

				// We support only http requests
				if (request_url.scheme() != "https" && request_url.scheme() != "http") {
					continue;
				}

				// Skip invalid and the same urls
				if (request_url.host().empty() || request_url.to_string() == reply.url().to_string())
					continue;

				// Check by user filters
				bool ok = true;
				for (auto jt = url_filters.begin(); ok && jt != url_filters.end(); ++jt) {
					ok &= (*jt)(reply, *it);
				}

				if (ok) {
					if (!inflight_insert(request_url))
						continue;

					auto rres = storage->read_data(request_url.to_string());
					rres.wait();
					if (rres.error().code()) {
						std::cout << "Page cache error (download from internet): url: " << request_url.to_string() <<
							", error: " << rres.error().message() << std::endl;
						download(request_url);
					} else {
						document doc = storage::unpack_document(rres.get_one().file());
						// document was stored before we started this update generation, process it again
						int will_process = dnet_time_before(&doc.ts, &generation_time);
						std::cout << "Url has been found in page cache: url: " << request_url.to_string() <<
							", will process (document was saved before current engine started): " << will_process <<
							std::endl;

						if (will_process) {
							found_in_page_cache(request_url, doc);

							if (reply.code() != ioremap::swarm::url_fetcher::response::not_modified) {
								for (auto it = processors.begin(); it != processors.end(); ++it)
									(*it)(reply, data, document_cache);
							}
						}
					}
				}
			}
		} else {
			for (auto it = fallback_processors.begin(); it != fallback_processors.end(); ++it)
				(*it)(reply, data, document_new);
		}

		for (auto && r : res) {
			r.wait();
			if (r.error()) {
				std::cout << "Document storage error: " << reply.request().url().to_string() << " " << r.error().message() << std::endl;
			}
		}
	}

	void process_url(const swarm::url_fetcher::response &reply, const std::string &data, const boost::system::error_code &error) {
		document old_doc = inflight_erase(reply.request().url());

		if (error) {
			std::cout << "Error  ... " << reply.request().url().to_string();
			if (reply.url().to_string() != reply.request().url().to_string())
				std::cout << " -> " << reply.url().to_string();
			std::cout << ": " << error.message() << std::endl;
			return;
		}

		if (reply.code() != ioremap::swarm::url_fetcher::response::not_modified) {
			process_reply(reply, data);
		} else {
			process_reply(reply, old_doc.data);
		}
	}
};

engine::engine() : m_data(new engine_data)
{
}

engine::~engine()
{
}

storage *engine::get_storage()
{
	return m_data->storage.get();
}

void engine::add_options(const boost::program_options::options_description &description)
{
	m_data->options.push_back(description);
}

boost::program_options::options_description_easy_init engine::add_options(const std::string &name)
{
	m_data->options.emplace_back(name);

	return m_data->options.back().add_options();
}

void engine::add_parser(const parser_functor &parser)
{
	m_data->parsers.push_back(parser);
}

void engine::add_filter(const filter_functor &filter)
{
	m_data->filters.push_back(filter);
}

void engine::add_url_filter(const url_filter_functor &filter)
{
	m_data->url_filters.push_back(filter);
}

void engine::add_processor(const process_functor &process)
{
	m_data->processors.push_back(process);
}

void engine::add_fallback_processor(const process_functor &process)
{
	m_data->fallback_processors.push_back(process);
}

void engine::show_help_message(std::ostream &out)
{
	out << m_data->command_line_options << std::endl;
}

int engine::parse_command_line(int argc, char **argv, boost::program_options::variables_map &vm)
{
	using namespace boost::program_options;

	options_description general_options("General options");

	std::vector<int> groups = { 1, 2, 3 };
	std::string group_string;
	std::string log_file;
	int log_level;
	std::string remote;
	std::string ns;
	int url_threads_count;

	general_options.add_options()
			("help", "This help message")
			("log-file", value<std::string>(&log_file)->default_value("/dev/stdout"), "Log file")
			("log-level", value<int>(&log_level)->default_value(DNET_LOG_ERROR), "Log level")
			("groups", value<std::string>(&group_string), "Groups which will host indexes and data, format: 1:2:3")
			("uthreads", value<int>(&url_threads_count)->default_value(3), "Number of URL downloading and processing threads")
			("namespace", value<std::string>(&ns), "Namespace for urls and indexes")
			("remote", value<std::string>(&remote),
			 "Remote node to connect, format: address:port:family (IPv4 - 2, IPv6 - 10)")
			;

	m_data->command_line_options.add(general_options);
	for (auto it = m_data->options.begin(); it != m_data->options.end(); ++it)
		m_data->command_line_options.add(*it);

	store(boost::program_options::parse_command_line(argc, argv, m_data->command_line_options), vm);
	notify(vm);

	if (vm.count("help") || !vm.count("remote")) {
		std::cerr << general_options << std::endl;
		for (auto it = m_data->options.begin(); it != m_data->options.end(); ++it)
			std::cerr << *it << std::endl;

		return -1;
	}

	if (group_string.size()) {
		struct digitizer {
			int operator() (const std::string &str) {
				return atoi(str.c_str());
			}
		};

		groups.clear();

		std::vector<std::string> gr;
		boost::split(gr, group_string, boost::is_any_of(":"));

		std::transform(gr.begin(), gr.end(), std::back_inserter<std::vector<int>>(groups), digitizer());
	}

	elliptics::file_logger log(log_file.c_str(), log_level);
	m_data->storage.reset(new wookie::storage(elliptics::node(log)));

	if (ns.size())
		m_data->storage->set_namespace(ns);

	try {
		m_data->storage->get_node().add_remote(remote.c_str());
	} catch (const elliptics::error &e) {
		std::cerr << "Could not connect to " << remote << ": " << e.what() << std::endl;
		return -1;
	}

	m_data->storage->set_groups(groups);

	m_data->downloader.reset(new wookie::dmanager(url_threads_count));

	return 0;
}

void engine::download(const swarm::url &url)
{
	m_data->download(url);
}

int engine::run()
{
	m_data->downloader->start();
	return 0;
}

void engine::found_in_page_cache(const std::string &url, const document &doc)
{
	m_data->found_in_page_cache(url, doc);
}

}}
