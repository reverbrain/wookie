#include <cocaine/framework/dispatch.hpp>
#include <wookie/application.hpp>
#include <wookie/parser.hpp>

using namespace ioremap::wookie;

/*!
 * \brief Extracts text out of document body
 * Takes document body as an input and extracts human readable text out of it
 * - Fills field "text" with parsed document body text
 */
class html_processor
{
public:
	html_processor(cocaine::framework::dispatch_t &d) :
		m_pipeline(d, "html_processor", "feature_extractor") {
		d.on<process_handler>("process", *this);
	}

	struct process_handler :
		public pipeline_process_handler<html_processor>,
		public std::enable_shared_from_this<process_handler>
	{
		process_handler(html_processor &parent) : pipeline_process_handler<html_processor>(parent)
		{
		}

		void on_request(meta_info_t &&info)
		{
			COCAINE_LOG_ERROR(parent().pipeline().logger(), "Processing html from page: %s", info.url());

			/*!
			 * Parse document body using standard wookie parser
			 */
			ioremap::wookie::parser parser;
			parser.feed_text(info.body());

			/*!
			 * Put parsed document text into field "text" in meta information
			 */
			info.set_value("text", parser.tokens());
			pipeline().push(shared_from_this(), info);
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
	return cocaine::framework::run<html_processor>(argc, argv);
}
