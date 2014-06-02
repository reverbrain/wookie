#include <cocaine/framework/dispatch.hpp>
#include <cocaine/framework/handlers/http.hpp>
#include <wookie/application.hpp>
#include <swarm/url.hpp>

using namespace ioremap::wookie;

/*!
 * \brief First processor in the pipeline
 * This processor starts the pipeline. In order to add document to pipeline you
 * can push it into application "first_processor".
 * Essentially, it can handle requests from native proxy via "update" method.
 * Also, it can be used from cocaine framework via "process" handle.
 * After receiving document it will push it into next application in pipeline.
 */
class processor
{
public:
	processor(cocaine::framework::dispatch_t &d) :
		m_pipeline(d, "first_processor", "html_processor") {
		d.on<process_handler>("process", *this);
		d.on<update_handler>("update", *this);
		d.on<echo_handler>("echo", *this);
	}

	/*!
	 * \brief Processor that handles "process" requests.
	 * Just sends passed meta_info_t info next pipeline application
	 */
	struct process_handler :
		public pipeline_process_handler<processor>,
		public std::enable_shared_from_this<process_handler>
	{
		process_handler(processor &parent) : pipeline_process_handler<processor>(parent)
		{
		}

		void on_request(meta_info_t &&info)
		{
			parent().pipeline().push(shared_from_this(), info);
		}
	};

	/*!
	 * \brief The update_handler struct makes possible to upload single document to pipeline
	 *
	 * To upload document to pipeline send POST request to url:
	 * http://proxy/first_processor/update?url=original_url
	 * with document's body as data.
	 */
	struct update_handler :
		public cocaine::framework::http_handler<processor>,
		public std::enable_shared_from_this<update_handler>
	{
		update_handler(processor &parent) : cocaine::framework::http_handler<processor>(parent)
		{
		}

		void on_request(const cocaine::framework::http_request_t &request)
		{
			COCAINE_LOG_ERROR(parent().pipeline().logger(), "Received request: %s", request.uri());

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
			info.set_url(url);
			info.set_body(body);

			auto that = shared_from_this();
			parent().pipeline().next()->push(url, info).then(
				[that, url] (cocaine::framework::generator<void> &future) {
				try {
					future.next();
				} catch (std::exception &e) {
					COCAINE_LOG_ERROR(that->parent().pipeline().logger(), "Failed to send to next processor, url: %s, error: %s", url, e.what());

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

	meta_info_pipeline_t &pipeline()
	{
		return m_pipeline;
	}

private:
	meta_info_pipeline_t m_pipeline;
};

int main(int argc, char *argv[])
{
	return cocaine::framework::run<processor>(argc, argv);
}
