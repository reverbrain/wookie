#include <cocaine/framework/dispatch.hpp>
#include <wookie/application.hpp>

using namespace ioremap::wookie;

class processor
{
public:
	processor(cocaine::framework::dispatch_t &d) :
		m_pipeline(d, "feature_extractor", "") {
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
			COCAINE_LOG_ERROR(parent().pipeline().logger(), "Finishing processing page: %s", info.url());

			/* Finish processing */
			pipeline().finish(shared_from_this(), info.url());

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
