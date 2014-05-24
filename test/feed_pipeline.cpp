#include "wookie/timer.hpp"
#include "wookie/application.hpp"

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
namespace cf = cocaine::framework;

auto manager = cf::service_manager_t::create(cf::service_manager_t::endpoint_t("127.0.0.1", 10053));
auto app = manager->get_service<cf::app_service_t>("first_processor");

struct request_handler_functor
{
	void operator() (const ioremap::swarm::url_fetcher::response &reply, const std::string &data,
			const boost::system::error_code &error) const {
		std::cout << "Request finished: " << reply.request().url().to_string() << " -> " << reply.url().to_string() << std::endl;
		std::cout << "HTTP code: " << reply.code() << std::endl;
		std::cout << "Error: " << error.message() << std::endl;

		wookie::meta_info_t meta_info;
		meta_info.set_url(reply.url().to_string());
		meta_info.set_body(data);
		auto g = app->enqueue("process", meta_info);
	}
};

int main(int argc, char **argv)
{
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " url" << std::endl;
		return 1;
	}

	boost::asio::io_service service;
	swarm::boost_event_loop loop(service);

	swarm::logger logger("/dev/stdout", ioremap::swarm::SWARM_LOG_INFO);

	swarm::url_fetcher manager(loop, logger);

	swarm::url_fetcher::request request;
	request.set_url(argv[1]);
	request.set_follow_location(1);
	request.set_timeout(1000);
	request.headers().assign({
		{ "Content-Type", "text/html; always" }
	});

	wookie::timer tm;

	request_handler_functor request_handler;

	manager.get(swarm::simple_stream::create(request_handler), std::move(request));

	boost::asio::io_service::work work(service);

	boost::asio::signal_set signals(service, SIGINT, SIGTERM);
	signals.async_wait(std::bind(&boost::asio::io_service::stop, &service));

	service.run();

	std::cout << "Finished in: " << tm.elapsed() << " ms" << std::endl;

	return 0;
}

