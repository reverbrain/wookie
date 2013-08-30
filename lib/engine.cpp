#include "wookie/engine.hpp"
#include "wookie/storage.hpp"
#include "wookie/dmanager.hpp"
#include "wookie/parser.hpp"
#include "wookie/lexical_cast.hpp"

#include <mutex>
#include <magic.h>

namespace ioremap { namespace wookie {

class magic_data {
	public:
		magic_data() {
			magic = magic_open(MAGIC_MIME);
			if (!magic)
				ioremap::elliptics::throw_error(-ENOMEM, "Failed to create MIME magic handler");

			if (magic_load(magic, 0) == -1) {
				magic_close(magic);
				ioremap::elliptics::throw_error(-ENOMEM, "Failed to load MIME magic database");
			}
		}

		~magic_data() {
			magic_close(magic);
		}

		const char *type(const char *buffer, size_t size) {
			const char *ret = magic_buffer(magic, buffer, size);

			if (!ret)
				ret = "none";

			return ret;
		}

		bool is_text(const char *buffer, size_t size) {
			return !strncmp(type(buffer, size), "text/", 5);
		}

	private:
		magic_t magic;
};

filter_functor create_text_filter()
{
	struct filter
	{
		magic_data magic;

		bool check(const swarm::network_reply &reply)
		{
			bool text = false;
			bool has_content_type = false;
			for (auto h : reply.get_headers()) {
				if (h.first == "Content-Type") {
					std::cout << h.second << std::endl;

					text = !strncmp(h.second.c_str(), "text/", 5);
					has_content_type = true;
					break;
				}
			}

			if (!has_content_type) {
				text = magic.is_text(reply.get_data().c_str(), reply.get_data().size());
			}

			return text;
		}
	};

	return std::bind(&filter::check, std::make_shared<filter>(), std::placeholders::_1);
}

url_filter_functor create_domain_filter(const std::string &url)
{
	struct filter
	{
		std::string base_host;

		filter(const std::string &url)
		{
			ioremap::swarm::network_url base_url;

			if (!base_url.set_base(url))
				ioremap::elliptics::throw_error(-EINVAL, "Invalid URL '%s': set-base failed", url.c_str());

			base_host = base_url.host();
			if (base_host.empty())
				ioremap::elliptics::throw_error(-EINVAL, "Invalid URL '%s': base is empty", url.c_str());
		}

		bool check(const swarm::network_reply &reply, const std::string &url)
		{
			std::string host;
			ioremap::swarm::network_url base_url;
			base_url.set_base(reply.get_url());
			base_url.relative(url, &host);

			return base_host == host;
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

		bool check(const std::string &url) {
			std::string::size_type pos = url.find(":");
			if (pos == std::string::npos)
				return true;
			if (pos == url.size() - 1)
				return true;

			int port = atoi(url.c_str() + pos + 1);

			for (auto it = allowed_ports.begin(); it != allowed_ports.end(); ++it) {
				if (*it == port)
					return true;
			}

			return false;
		}
	};

	return std::bind(&filter::check, std::make_shared<filter>(ports), std::placeholders::_2);
}

parser_functor create_href_parser()
{
	struct parser
	{
		std::vector<std::string> operator() (const swarm::network_reply &reply)
		{
			wookie::parser p;
			p.parse(reply.get_data());

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
	magic_data magic;

	struct dnet_time generation_time;

	engine_data() : total(0) {
		dnet_current_time(&generation_time);
	}

	void download(const std::string &url) {
		std::cout << "Downloading ... " << url << std::endl;
		downloader->feed(url, std::bind(&engine_data::process_url, this, std::placeholders::_1));
	}

	void found_in_page_cache(const std::string &url, const document &doc) {
		std::cout << "Downloading (if-modified-since " << doc.ts << ") ... " << url << std::endl;
		inflight_insert(url, doc);
		downloader->feed(url, doc, std::bind(&engine_data::process_url, this, std::placeholders::_1));
	}

	bool inflight_insert(const std::string &url, const document &doc) {
		std::unique_lock<std::mutex> guard(inflight_lock);

		auto check = inflight.find(url);
		if (check == inflight.end()) {
			inflight.insert(std::make_pair(url, doc));
			return true;
		} else {
			check->second = doc;
		}

		return false;
	}

	bool inflight_insert(const std::string &url) {
		return inflight_insert(url, document());
	}

	document inflight_erase(const std::string &url) {
		document d;

		std::unique_lock<std::mutex> guard(inflight_lock);
		auto check = inflight.find(url);
		if (check != inflight.end()) {
			d = check->second;
			inflight.erase(check);
		}

		return d;
	}

	ioremap::elliptics::async_write_result store_document(const std::string &url, const std::string &content, const dnet_time &ts) {
		wookie::document d;
		d.ts = ts;
		d.key = url;
		d.data = content;

		return storage->write_document(d);
	}

	void process_reply(const swarm::network_reply &reply) {
		std::cout << "Processing  ... " << reply.get_request().get_url();
		if (reply.get_url() != reply.get_request().get_url())
			std::cout << " -> " << reply.get_url();

		std::cout << ", code: " << reply.get_code() <<
			     ", total-urls: " << total <<
			     ", data-size: " << reply.get_data().size() <<
			     ", headers: " << reply.get_headers().size() <<
			     std::endl;

		bool accepted_by_filters = true;
		for (auto it = filters.begin(); accepted_by_filters && it != filters.end(); ++it) {
			accepted_by_filters &= (*it)(reply);
		}

		++total;
		std::list<ioremap::elliptics::async_write_result> res;

		struct dnet_time ts;
		dnet_current_time(&ts);

		res.emplace_back(store_document(reply.get_url(), reply.get_data(), ts));

		// if original URL redirected to other location, store object by original URL too
		if (reply.get_url() != reply.get_request().get_url())
			res.emplace_back(store_document(reply.get_request().get_url(), reply.get_url(), ts));

		if (accepted_by_filters) {
			if (reply.get_code() != ioremap::swarm::network_reply::not_modified) {
				for (auto it = processors.begin(); it != processors.end(); ++it)
					(*it)(reply, document_new);
			}

			std::vector<std::string> urls;

			for (auto it = parsers.begin(); it != parsers.end(); ++it) {
				const auto new_urls = (*it)(reply);
				urls.insert(urls.end(), new_urls.begin(), new_urls.end());
			}

			std::sort(urls.begin(), urls.end());
			urls.erase(std::unique(urls.begin(), urls.end()), urls.end());

			swarm::network_url base_url;
			base_url.set_base(reply.get_url());

			for (auto it = urls.begin(); it != urls.end(); ++it) {
				std::string host;
				std::string request_url = base_url.relative(*it, &host);

				// We support only http requests
				if ((request_url.compare(0, 6, "https:") != 0) && (request_url.compare(0, 5, "http:") != 0))
					continue;

				// Skip invalid and the same urls
				if (request_url.empty() || host.empty() || (request_url == reply.get_url()))
					continue;

				// Check by user filters
				bool ok = true;
				for (auto jt = url_filters.begin(); ok && jt != url_filters.end(); ++jt) {
					ok &= (*jt)(reply, *it);
				}

				if (ok) {
					if (!inflight_insert(request_url))
						continue;

					auto rres = storage->read_data(request_url);
					rres.wait();
					if (rres.error().code()) {
						std::cout << "Page cache error (download from internet): url: " << request_url <<
							", error: " << rres.error().message() << std::endl;
						download(request_url);
					} else {
						document doc = storage::unpack_document(rres.get_one().file());
						// document was stored before we started this update generation, process it again
						int will_process = dnet_time_before(&doc.ts, &generation_time);
						std::cout << "Url has been found in page cache: url: " << request_url <<
							", will process (document was saved before current engine started): " << will_process <<
							std::endl;

						if (will_process) {
							found_in_page_cache(request_url, doc);

							if (reply.get_code() != ioremap::swarm::network_reply::not_modified) {
								for (auto it = processors.begin(); it != processors.end(); ++it)
									(*it)(reply, document_cache);
							}
						}
					}
				}
			}
		} else {
			for (auto it = fallback_processors.begin(); it != fallback_processors.end(); ++it)
				(*it)(reply, document_new);
		}

		for (auto && r : res) {
			r.wait();
			if (r.error()) {
				std::cout << "Document storage error: " << reply.get_request().get_url() << " " << r.error().message() << std::endl;
			}
		}
	}

	void process_url(const swarm::network_reply &reply) {
		document old_doc = inflight_erase(reply.get_request().get_url());

		if (reply.get_error()) {
			std::cout << "Error  ... " << reply.get_request().get_url();
			if (reply.get_url() != reply.get_request().get_url())
				std::cout << " -> " << reply.get_url();
			std::cout << ": " << reply.get_error() << std::endl;
			return;
		}

		if (reply.get_code() != ioremap::swarm::network_reply::not_modified) {
			process_reply(reply);
		} else {
			swarm::network_reply r(reply);

			r.set_data(old_doc.data);
			process_reply(r);
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
	elliptics::node node(log, ns);
	elliptics::session sess(node);
	m_data->storage.reset(new wookie::storage(sess));

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

void engine::download(const std::string &url)
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
