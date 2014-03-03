#include "wookie/timer.hpp"

#include <swarm/urlfetcher/url_fetcher.hpp>
#include <swarm/urlfetcher/boost_event_loop.hpp>
#include <swarm/urlfetcher/stream.hpp>
#include <swarm/logger.hpp>
#include <swarm/c++config.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <list>
#include <thread>

#include <boost/asio/signal_set.hpp>

using namespace ioremap;

struct request_handler_functor
{
	void operator() (const ioremap::swarm::url_fetcher::response &reply, const std::string &data,
			const boost::system::error_code &error) const {
		std::cout << "Request finished: " << reply.request().url().to_string() << " -> " << reply.url().to_string() << std::endl;
		std::cout << "HTTP code: " << reply.code() << std::endl;
		std::cout << "Error: " << error.message() << std::endl;

		const auto &headers = reply.headers().all();

		for (auto it = headers.begin(); it != headers.end(); ++it) {
			std::cout << "header: \"" << it->first << "\": \"" << it->second << "\"" << std::endl;
		}
		(void) data;
	}
};

int main(int argc, char **argv)
{
	boost::asio::io_service service;
	swarm::boost_event_loop loop(service);

	swarm::logger logger("/dev/stdout", ioremap::swarm::SWARM_LOG_DEBUG);

	swarm::url_fetcher manager(loop, logger);

	swarm::url_fetcher::request request;
	request.set_url("http://www.yandex.ru");
	request.set_follow_location(1);
	request.set_timeout(1000);
	request.headers().assign({
		{ "Content-Type", "text/html; always" },
		{ "Additional-Header", "Very long-long\r\n\tsecond line\r\n\tthird line" }
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
