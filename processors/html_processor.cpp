#include <cocaine/framework/dispatch.hpp>
#include <wookie/application.hpp>
#include <wookie/parser.hpp>

using namespace ioremap::wookie;

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
			ioremap::wookie::parser parser;
			parser.feed_text(info.body());
			info.set_value("text", parser.text());
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


