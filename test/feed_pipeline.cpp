#include "wookie/timer.hpp"
#include "wookie/application.hpp"
#include "wookie/parser.hpp"
#include "wookie/dmanager.hpp"
#include "wookie/engine.hpp"

#include <swarm/urlfetcher/url_fetcher.hpp>
#include <swarm/urlfetcher/boost_event_loop.hpp>
#include <swarm/urlfetcher/stream.hpp>
#include <swarm/logger.hpp>
#include <swarm/c++config.hpp>

#include <cocaine/framework/services/app.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <list>
#include <thread>

#include <boost/asio/signal_set.hpp>

using namespace ioremap;
using namespace ioremap::wookie;
namespace cf = cocaine::framework;

auto manager = cf::service_manager_t::create(cf::service_manager_t::endpoint_t("127.0.0.1", 10053));
auto app = manager->get_service<cf::app_service_t>("first_processor");

struct feed_pipeline_processor
{
	wookie::engine &engine;
	std::string base;
	bool fallback;

	feed_pipeline_processor(wookie::engine &engine, const std::string &base, bool fallback)
		: engine(engine), base(base), fallback(fallback) {
	}

	void process_text(const ioremap::swarm::url_fetcher::response &reply, const std::string &data, document_type) {
		try {
			wookie::meta_info_t meta_info;
			meta_info.set_url(reply.url().to_string());
			meta_info.set_body(data);
			auto g = app->enqueue("process", meta_info);
		} catch (const std::exception &e) {
			std::cerr << reply.url().to_string() << ": feed pipeline exception: " << e.what() << std::endl;
			engine.download(reply.request().url());
		}
	}

	static process_functor create(wookie::engine &engine, const std::string &url, bool fallback) {
		ioremap::swarm::url base_url = url;
		if (!base_url.is_valid())
			ioremap::elliptics::throw_error(-EINVAL, "Invalid URL '%s': set-base failed", url.c_str());

		std::string base = base_url.host();
		if (base.empty())
			ioremap::elliptics::throw_error(-EINVAL, "Invalid URL '%s': base is empty", url.c_str());

		return std::bind(&feed_pipeline_processor::process_text,
			std::make_shared<feed_pipeline_processor>(engine, base, fallback),
			std::placeholders::_1,
			std::placeholders::_2,
			std::placeholders::_3);
	}
};

url_filter_functor create_words_filter(const std::vector<std::string> &forbidden_words)
{
	struct filter
	{
		std::vector<std::string> forbidden_words;

		filter(const std::vector<std::string> &forbidden_words) : forbidden_words(forbidden_words) {
		}

		bool check(const swarm::url &url) {
			const std::string url_string = url.to_string();
			for (auto &w : forbidden_words) {
				std::string::size_type pos = url_string.find(w);
				if ((pos != std::string::npos) && (pos != url_string.size() - 1))
					return false;
			}

			return true;
		}
	};

	return std::bind(&filter::check, std::make_shared<filter>(forbidden_words), std::placeholders::_2);
}


int main(int argc, char *argv[])
{
	using namespace boost::program_options;

	std::string url;
	variables_map vm;
	wookie::engine engine;

	engine.add_options("Feed pipeline options")
		("url", value<std::string>(&url), "Url to download")
	;

	try {
		int err = engine.parse_command_line(argc, argv, vm);
		if (err < 0)
			return err;
	} catch (const std::exception &e) {
		std::cerr << "Command line parsing failed: " << e.what() << std::endl;
		engine.show_help_message(std::cerr);
		return -1;
	}

	if (!vm.count("url")) {
		std::cerr << "You must provide URL" << std::endl;
		engine.show_help_message(std::cerr);
		return -1;
	}

	try {
		std::vector<int> allowed_ports = { 80, 8080 };

		std::vector<std::string> forbidden_words;
		forbidden_words.push_back("/blog");

		engine.add_filter(create_text_filter());
		engine.add_url_filter(create_domain_filter(url));
		engine.add_url_filter(create_words_filter(forbidden_words));
		engine.add_url_filter(create_port_filter(allowed_ports));
		engine.add_parser(create_href_parser());
		engine.add_processor(feed_pipeline_processor::create(engine, url, false));
		engine.add_fallback_processor(feed_pipeline_processor::create(engine, url, true));

		engine.download(url);

		return engine.run();
	} catch (const std::exception &e) {
		std::cerr << "main thread exception: " << e.what() << std::endl;
	}
	return 0;
}
