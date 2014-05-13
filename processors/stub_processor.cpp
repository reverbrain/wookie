#include <cocaine/framework/dispatch.hpp>
#include <wookie/application.hpp>

using namespace ioremap::wookie;

class processor
{
public:
	processor(cocaine::framework::dispatch_t &d) :
		m_pipeline(d, "stub_processor", "last_processor") {
		d.on<process_handler>("process", *this);
	}

	struct process_handler :
		public pipeline_process_handler<processor>,
		public std::enable_shared_from_this<process_handler>
	{
		process_handler(processor &parent) : pipeline_process_handler<processor>(parent)
		{
		}

		void on_request(meta_info_t &&info)
		{
			/* Do some magic processing */
			info.set_value("stub", std::string("stub-info"));

			/* Send to next processor */
			parent().pipeline().push(shared_from_this(), info);

			/* Yes, that is all, pipeline will close upstream and do all the stuff */
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

