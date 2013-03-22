#include <fstream>
#include <vector>
#include <sstream>

#include <boost/program_options.hpp>

#include "wookie/split.hpp"
#include <elliptics/cppdef.h>

using namespace ioremap;
namespace ell = ioremap::elliptics;

class wookie_storage : public ioremap::elliptics::node {
	public:
		explicit wookie_storage(ell::logger &log) : node(log) {
		}

		void set_groups(const std::vector<int> groups) {
			m_groups = groups;
		}

		void process_file(const std::string &file) {
			std::ifstream in(file.c_str());
			std::ostringstream ss;
			ss << in.rdbuf();

			process(ss.str());
		}

		void process(const std::string &data) {
			wookie::mpos_t pos = m_spl.feed(data);
			for (auto p : pos) {
				std::cout << p.first << " : " << p.second.size() << std::endl;
			}
		}

	private:
		std::vector<int> m_groups;
		wookie::split m_spl;
};

int main(int argc, char *argv[])
{
	namespace po = boost::program_options;
	po::options_description desc("Options (required options are marked with *)");

	int groups_array[] = {1, 2, 3};
	std::vector<int> groups(groups_array, groups_array + ARRAY_SIZE(groups_array));
	std::string log_file;
	int log_level;
	std::string file;
	std::string remote;

	desc.add_options()
		("help", "This help message")
		("log-file", po::value<std::string>(&log_file)->default_value("/dev/stdout"), "Log file")
		("log-level", po::value<int>(&log_level)->default_value(DNET_LOG_INFO), "Log level")
		("groups", po::value<std::vector<int> >(&groups), "Groups which will host indexes and data, format: 1:2:3")
		("file", po::value<std::string>(&file), "Input text file")
		("remote", po::value<std::string>(&remote), "Remote node to connect, format: address:port:family (IPv4 - 2, IPv6 - 10)")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count("help") || !vm.count("file") || !vm.count("remote")) {
		std::cerr << desc << std::endl;
		return -1;
	}


	ell::file_logger log(log_file.c_str(), log_level);
	wookie_storage st(log);

	try {
		st.add_remote(remote.c_str());
	} catch (const ell::error &e) {
		std::cerr << "Could not connect to " << remote << ": " << e.what() << std::endl;
		return -1;
	}

	boost::locale::generator gen;
	std::locale loc=gen("en_US.UTF8"); 
	std::locale::global(loc);


	st.set_groups(groups);
	st.process_file(file);
}
