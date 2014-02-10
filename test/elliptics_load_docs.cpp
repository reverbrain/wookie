#include "similarity.hpp"
#include "simdoc.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

using namespace ioremap;
using namespace ioremap::similarity;

class loader {
	public:
		loader(const std::string &remote, const std::string &group_string, const std::string &log, int level) :
		m_done(false),
		m_logger(log.c_str(), level),
		m_node(m_logger) {
			m_node.add_remote(remote.c_str());

			struct digitizer {
				int operator() (const std::string &str) {
					return atoi(str.c_str());
				}
			};

			std::vector<std::string> gr;
			boost::split(gr, group_string, boost::is_any_of(":"));

			std::transform(gr.begin(), gr.end(), std::back_inserter<std::vector<int>>(m_groups), digitizer());
		}

		void load(const std::string &index, const std::string &train_file) {
			std::vector<std::string> vindexes;
			vindexes.push_back(index);

			elliptics::session session(m_node);
			session.set_groups(m_groups);
			session.set_exceptions_policy(elliptics::session::no_exceptions);
			session.set_ioflags(DNET_IO_FLAGS_CACHE);

			std::vector<elliptics::key> keys;

			auto indexes = session.find_all_indexes(vindexes);
			for (auto idx = indexes.begin(); idx != indexes.end(); ++idx) {
				keys.push_back(idx->id);
			}

			m_documents.reserve(keys.size());

			session.bulk_read(keys).connect(std::bind(&loader::result_callback, this, std::placeholders::_1),
					std::bind(&loader::final_callback, this, std::placeholders::_1));

			while (!m_done) {
				std::unique_lock<std::mutex> guard(m_cond_lock);
				m_cond.wait(guard);
			}
		}

	private:
		std::mutex m_lock;
		std::vector<simdoc> m_documents;

		std::mutex m_cond_lock;
		std::condition_variable m_cond;
		bool m_done;

		elliptics::file_logger m_logger;
		elliptics::node m_node;
		std::vector<int> m_groups;

		void result_callback(const elliptics::read_result_entry &result) {
			if (result.size()) {
				try {
					msgpack::unpacked msg;
					const elliptics::data_pointer &tmp = result.file();

					msgpack::unpack(&msg, tmp.data<char>(), tmp.size());

					simdoc doc;
					msg.get().convert(&doc);

					std::unique_lock<std::mutex> guard(m_lock);
					m_documents.emplace_back(doc);
				} catch (const std::exception &e) {
					fprintf(stderr, "exception: id: %s, object-size: %zd, error: %s\n",
							dnet_dump_id_str(result.io_attribute()->id), result.size(), e.what());
							
				}
			}
		}

		void final_callback(const elliptics::error_info &error) {
			printf("loaded: %zd docs, error: %d\n", m_documents.size(), error.code());
			m_done = true;
			m_cond.notify_one();
		}
};

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("Similarity options");

	std::string train_file, index;
	std::string remote, group_string;
	std::string log_file;
	int log_level;

	generic.add_options()
		("help", "This help message")
		("model-file", bpo::value<std::string>(&train_file)->required(), "ML model file")
		("index", bpo::value<std::string>(&index)->required(), "Elliptics index for loaded objects")

		("remote", bpo::value<std::string>(&remote)->required(), "Remote elliptics server")
		("groups", bpo::value<std::string>(&group_string)->required(), "Colon seaprated list of groups")
		("log", bpo::value<std::string>(&log_file)->default_value("/dev/stdout"), "Elliptics log file")
		("log-level", bpo::value<int>(&log_level)->default_value(DNET_LOG_INFO), "Elliptics log-level")
		;

	bpo::variables_map vm;

	try {
		bpo::store(bpo::parse_command_line(argc, argv, generic), vm);
		if (vm.count("help")) {
			std::cout << generic << std::endl;
			return 0;
		}

		bpo::notify(vm);
	} catch (const std::exception &e) {
		std::cerr << "Invalid options: " << e.what() << "\n" << generic << std::endl;
		return -1;
	}

	try {
		loader el(remote, group_string, log_file, log_level);
		el.load(index, train_file);

	} catch (const std::exception &e) {
		std::cerr << "exception: " << e.what() << std::endl;
		return -1;
	}
}
