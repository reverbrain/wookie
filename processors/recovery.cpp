#include <cocaine/framework/dispatch.hpp>
#include <cocaine/framework/handlers/http.hpp>
#include <wookie/application.hpp>
#include <swarm/url.hpp>

using namespace ioremap::wookie;

class processor
{
public:
	processor(cocaine::framework::dispatch_t &d) : m_tags(1, "documents"), m_dispatch(d)
	{
		m_logger = d.service_manager()->get_system_logger();
		m_storage = d.service_manager()->get_service<cocaine::framework::storage_service_t>("storage");

		d.on<recovery_handler>("recover", *this);
	}

	/*!
	 * \brief The recovery_handler struct makes possible to upload single document to pipeline
	 *
	 * To upload document to pipeline send POST request to url:
	 * http://proxy/recovery/recover?processor=first_processor&count=16
	 * with empty body.
	 *
	 * Where \a processor is the name of processor (application name, like html_processor) to recover.
	 * There will be \a count independent connections to \a processor application.
	 */
	struct recovery_handler :
		public cocaine::framework::http_handler<processor>,
		public std::enable_shared_from_this<recovery_handler>
	{
		recovery_handler(processor &parent) : cocaine::framework::http_handler<processor>(parent)
		{
		}

		void on_request(const cocaine::framework::http_request_t &request)
		{
			COCAINE_LOG_ERROR(parent().logger(), "Received request: %s", request.uri());

			cocaine::framework::http_headers_t headers = request.headers();
			headers.reset_header("Content-Length", std::to_string(request.uri().size()));

			const ioremap::swarm::url original_url(request.uri());
			const ioremap::swarm::url_query &query = original_url.query();

			auto processor_name = query.item_value("processor");
			const auto connections_count = query.item_value("count", 1u);

			if (!processor_name || connections_count < 1) {
				cocaine::framework::http_headers_t headers;
				headers.add_header("Content-Length", "0");
				response()->write_headers(400, headers);
				response()->close();
				return;
			}

			auto that = shared_from_this();

			//! Create \a connections_count connections to application
			try {
				for (unsigned i = 0; i < connections_count; ++i) {
					processors.push_back(parent().service_manager()->get_service<processor_t>(*processor_name, parent().storage()));
				}
			} catch (std::exception &e) {
				COCAINE_LOG_ERROR(that->parent().logger(), "Failed to connect to processor: %s, error: %s", *processor_name, e.what());

				cocaine::framework::http_headers_t headers;
				headers.add_header("Content-Length", "0");
				that->response()->write_headers(500, headers);
				that->response()->close();
				return;
			}

			//! Request all unprocessed urls from secondary index 'documents' in \a processor's \a name namespace
			parent().storage()->find(*processor_name, parent().tags()).then([this, that] (cocaine::framework::generator<std::vector<std::string>> &future) {
				try {
					urls = future.next();

					//! Send header to user, we don't want to make him worried
					//! that is why we will send him a byte per each processed url
					cocaine::framework::http_headers_t headers;
					headers.add_header("Content-Length", std::to_string(urls.size()));
					that->response()->write_headers(200, headers);

					for (auto processor : processors)
						process_next(processor);
				} catch (std::exception &e) {
					COCAINE_LOG_ERROR(parent().logger(), "Failed to retrieve the list of actions to restore, error: %s", e.what());
				}
			});
		}

		void process_next(const std::shared_ptr<processor_t> &processor)
		{
			if (urls.empty()) {
				finish_processor(processor);
				return;
			}

			auto url = urls.back();
			urls.pop_back();

			//! Retrieve url's meta info from storage and push it to processor
			auto that = shared_from_this();
			parent().storage()->read(processor->name(), url).then([that, processor, url] (cocaine::framework::generator<std::string> &future) {
				try {
					auto data = future.next();
					//! Push meta-info to target processor. It will remove url from secondary index
					//! itself when data will be finally processed
					processor->push_raw({ data.data(), data.size() }).then([that, processor, url] (cocaine::framework::generator<std::string> &future) {
						//! Send user a byte 's' if url was processed successfully and 'f' if not
						try {
							future.next();
							try {
								that->response()->write("s");
							} catch (...) {
							}
						} catch (std::exception &e) {
							COCAINE_LOG_ERROR(that->parent().logger(), "Failed to process, processor: %s, url: %s, error: %s", processor->name(), url, e.what());

							try {
								that->response()->write("f");
							} catch (...) {
							}
						}

						that->process_next(processor);
					});
				} catch (std::exception &e) {
					COCAINE_LOG_ERROR(that->parent().logger(), "Failed to retrieve metadata, processor: %s, url: %s, error: %s", processor->name(), url, e.what());
				}
			});
		}

		void finish_processor(const std::shared_ptr<processor_t> &processor)
		{
			auto it = std::find(processors.begin(), processors.end(), processor);
			if (it != processors.end())
				processors.erase(it);

			//! We were the last? Tell the user that we have finished
			if (processors.empty())
				response()->close();
		}

		std::vector<std::shared_ptr<processor_t>> processors;
		std::vector<std::string> urls;
	};

	const std::vector<std::string> &tags() const
	{
		return m_tags;
	}

	const std::shared_ptr<cocaine::framework::storage_service_t> &storage() const
	{
		return m_storage;
	}

	const std::shared_ptr<cocaine::framework::logger_t> &logger() const
	{
		return m_logger;
	}

	std::shared_ptr<cocaine::framework::service_manager_t> service_manager() const
	{
		return m_dispatch.service_manager();
	}

private:
	const std::vector<std::string> m_tags;
	cocaine::framework::dispatch_t &m_dispatch;
	std::shared_ptr<cocaine::framework::logger_t> m_logger;
	std::shared_ptr<cocaine::framework::storage_service_t> m_storage;
	std::map<std::string, std::shared_ptr<processor_t>> m_processors;
};

int main(int argc, char *argv[])
{
	return cocaine::framework::run<processor>(argc, argv);
}

