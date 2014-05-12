#include <cocaine/framework/dispatch.hpp>
#include <cocaine/framework/handlers/http.hpp>
#include <wookie/application.hpp>
#include <swarm/url.hpp>

using namespace ioremap::wookie;

class processor
{
public:
	processor(cocaine::framework::dispatch_t &d) {
		d.on<process_handler>("process", *this);
		d.on<update_handler>("update", *this);
		d.on<echo_handler>("echo", *this);
		m_logger = d.service_manager()->get_system_logger();
		m_storage = d.service_manager()->get_service<cocaine::framework::storage_service_t>("storage");
		m_next = d.service_manager()->get_service<processor_t>("last_processor", m_storage);
	}

	struct process_handler :
		public cocaine::framework::handler<processor>,
		public std::enable_shared_from_this<process_handler>
	{
		process_handler(processor &calc) : cocaine::framework::handler<processor>(calc)
		{
		}

		void on_chunk(const char *chunk, size_t size)
		{
			auto info = cocaine::framework::unpack<meta_info_t>(chunk, size);
			auto url = info["url"];

			auto that = shared_from_this();
			parent().next()->push(url, info).then(
				[that, url] (cocaine::framework::generator<void> &future) {
				try {
					future.next();
				} catch (std::exception &e) {
					COCAINE_LOG_ERROR(that->parent().logger(), "Failed to send to next processor, url: %s, error: %s", url, e.what());

					that->response()->error(100, "Failed to send to next processor");
					return;
				}

				that->parent().storage()->remove("first_processor", url);
				that->response()->close();
			});
		}
	};

	struct update_handler :
		public cocaine::framework::http_handler<processor>,
		public std::enable_shared_from_this<update_handler>
	{
		update_handler(processor &calc) : cocaine::framework::http_handler<processor>(calc)
		{
		}

		void on_request(const cocaine::framework::http_request_t &request)
		{
			COCAINE_LOG_ERROR(parent().logger(), "Received request: %s", request.uri());

			cocaine::framework::http_headers_t headers = request.headers();
			headers.reset_header("Content-Length", std::to_string(request.uri().size()));

			const ioremap::swarm::url original_url(request.uri());
			const ioremap::swarm::url_query &query = original_url.query();

			auto that_url = query.item_value("url");

			if (!that_url) {
				cocaine::framework::http_headers_t headers;
				headers.add_header("Content-Length", "0");
				response()->write_headers(400, headers);
				response()->close();
				return;
			}

			auto url = *that_url;
			const std::string &body = request.body();

			meta_info_t info;
			info["url"] = url;
			info["body"] = body;

			auto that = shared_from_this();
			parent().next()->push(url, info).then(
				[that, url] (cocaine::framework::generator<void> &future) {
				try {
					future.next();
				} catch (std::exception &e) {
					COCAINE_LOG_ERROR(that->parent().logger(), "Failed to send to next processor, url: %s, error: %s", url, e.what());

					cocaine::framework::http_headers_t headers;
					headers.add_header("Content-Length", "0");
					that->response()->write_headers(500, headers);
					that->response()->close();
					return;
				}

				cocaine::framework::http_headers_t headers;
				headers.add_header("Content-Length", "0");
				that->response()->write_headers(200, headers);
				that->response()->close();
			});
		}
	};

	struct echo_handler :
		public cocaine::framework::handler<processor>,
		public std::enable_shared_from_this<echo_handler>
	{
		echo_handler(processor &calc) : cocaine::framework::handler<processor>(calc)
		{
		}

		void on_chunk(const char *chunk, size_t size)
		{
			cocaine::framework::http_request_t request = cocaine::framework::unpack<cocaine::framework::http_request_t>(chunk, size);
			int code = 200;
			cocaine::framework::http_headers_t headers = request.headers();
			headers.reset_header("content-length", std::to_string(request.uri().size()));

			response()->write(std::make_tuple(code, headers));
			response()->write(request.uri());
			response()->close();
		}
	};

	const std::shared_ptr<processor_t> &next() const
	{
		return m_next;
	}

	const std::shared_ptr<cocaine::framework::storage_service_t> &storage() const
	{
		return m_storage;
	}

	const std::shared_ptr<cocaine::framework::logger_t> &logger() const
	{
		return m_logger;
	}

private:
	std::shared_ptr<processor_t> m_next;
	std::shared_ptr<cocaine::framework::storage_service_t> m_storage;
	std::shared_ptr<cocaine::framework::logger_t> m_logger;
};

int main(int argc, char *argv[])
{
	return cocaine::framework::run<processor>(argc, argv);
}
