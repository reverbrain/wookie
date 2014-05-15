#include <cocaine/framework/dispatch.hpp>
#include <wookie/application.hpp>

using namespace ioremap::wookie;

class feature_extractor
{
public:
	feature_extractor(cocaine::framework::dispatch_t &d) :
		m_pipeline(d, "feature_extractor", "last_processor") {
		d.on<process_handler>("process", *this);
	}

	struct process_handler :
		public pipeline_process_handler<feature_extractor>,
		public std::enable_shared_from_this<process_handler>
	{
		process_handler(feature_extractor &parent) : pipeline_process_handler<feature_extractor>(parent)
		{
		}

		void on_request(meta_info_t &&info)
		{
			std::stringstream ss(info.value<std::string>("text"));
			std::unordered_map<std::string, size_t> terms_frequencies;
			std::string word;
			while (ss >> word) {
				++terms_frequencies[word];
			}

			std::vector<std::pair<std::string, size_t>> terms_frequencies_vector(
				terms_frequencies.begin(), terms_frequencies.end()
			);
			info.set_value("terms_frequencies", terms_frequencies_vector);
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
	return cocaine::framework::run<feature_extractor>(argc, argv);
}



