#include "similarity.hpp"

#include "elliptics/session.hpp"

#include <list>
#include <thread>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include <msgpack.hpp>

using namespace ioremap;
using namespace ioremap::similarity;

struct simdoc {
	enum {
		version = 1,
	};

	int id;
	std::string text;
	std::vector<ngram> ngrams;
};

namespace msgpack
{
static inline simdoc &operator >>(msgpack::object o, simdoc &doc)
{
	if (o.type != msgpack::type::ARRAY || o.via.array.size != 4)
		ioremap::elliptics::throw_error(-EPROTO, "msgpack: simdoc array size mismatch: compiled: %d, unpacked: %d",
				4, o.via.array.size);

	object *p = o.via.array.ptr;

	int version;
	p[0].convert(&version);

	if (version != simdoc::version)
		ioremap::elliptics::throw_error(-EPROTO, "msgpack: simdoc version mismatch: compiled: %d, unpacked: %d",
				simdoc::version, version);

	p[1].convert(&doc.id);
	p[2].convert(&doc.text);
	p[3].convert(&doc.ngrams);

	return doc;
}

template <typename Stream>
static inline msgpack::packer<Stream> &operator <<(msgpack::packer<Stream> &o, const simdoc &d)
{
	o.pack_array(4);
	o.pack(static_cast<int>(simdoc::version));
	o.pack(d.id);
	o.pack(d.text);
	o.pack(d.ngrams);

	return o;
}

} /* namespace msgpack */


class loader {
	public:
		loader(const std::string &remote, const std::string &group_string, const std::string &log, int level) :
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

		void load(const std::string &index, const std::string &input_dir, const std::string &learn_file) {
			std::ifstream in(learn_file.c_str());

			m_indexes.clear();
			m_indexes.push_back(index);
			m_input_dir = input_dir;

			std::set<int> ids;

			std::string line;
			int line_num = 0;
			while (std::getline(in, line)) {
				if (!in.good())
					break;

				line_num++;

				int doc[2];

				int num = sscanf(line.c_str(), "%d\t%d\t", &doc[0], &doc[1]);
				if (num != 2) {
					fprintf(stderr, "failed to parse string: %d, tokens found: %d\n", line_num, num);
					break;
				}

				const char *pos = strrchr(line.c_str(), '\t');
				if (!pos) {
					fprintf(stderr, "could not find last tab delimiter\n");
					break;
				}

				pos++;
				if (pos && *pos) {
					learn_element le;

					le.doc_ids = std::vector<int>(doc, doc+2);
					le.request.assign(pos);

					ids.insert(doc[0]);
					ids.insert(doc[1]);
				}
			}

			m_doc_ids.assign(ids.begin(), ids.end());

			int num = std::thread::hardware_concurrency();
			if (num == 0)
				num = sysconf(_SC_NPROCESSORS_ONLN);

			add_documents(num);
		}

	private:
		std::vector<int> m_doc_ids;

		std::vector<std::string> m_indexes;
		std::string m_input_dir;

		elliptics::file_logger m_logger;
		elliptics::node m_node;
		std::vector<int> m_groups;

		struct doc_thread {
			int id;
			int step;
		};

		void load_documents(struct doc_thread &dth) {
			document_parser parser;
			elliptics::session session(m_node);

			session.set_groups(m_groups);
			session.set_exceptions_policy(elliptics::session::no_exceptions);

			for (size_t i = dth.id; i < m_doc_ids.size(); i += dth.step) {
				simdoc doc;
				doc.id = m_doc_ids[i];

				std::string doc_id_str = lexical_cast(doc.id);
				std::string file = m_input_dir + doc_id_str + ".html";
				try {
					parser.feed(file.c_str(), "");

					doc.text = parser.text();
					parser.generate_ngrams(doc.text, doc.ngrams);

					msgpack::sbuffer buffer;
					msgpack::pack(&buffer, doc);

					session.write_data(doc_id_str, elliptics::data_pointer::copy(buffer.data(), buffer.size()), 0)
						.connect(std::bind(&loader::update_index, this, session, doc_id_str));
				} catch (const std::exception &e) {
					std::cerr << file << ": caught exception: " << e.what() << std::endl;
				}
			}
		}

		void update_index(elliptics::session &session, const std::string &doc_id_str) {
			std::vector<elliptics::data_pointer> tmp;
			tmp.resize(m_indexes.size());
			session.set_indexes(doc_id_str, m_indexes, tmp);
		}

		void add_documents(int cpunum) {
			std::vector<std::thread> threads;

			wookie::timer tm;

			for (int i = 0; i < cpunum; ++i) {
				struct doc_thread dth;

				dth.id = i;
				dth.step = cpunum;

				threads.emplace_back(std::bind(&loader::load_documents, this, dth));
			}

			for (int i = 0; i < cpunum; ++i) {
				threads[i].join();
			}

			threads.clear();
			long docs_loading_time = tm.restart();

			printf("documents: %zd, load-time: %ld msec\n", m_doc_ids.size(), docs_loading_time);
		}
};

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("Similarity options");

	std::string input_dir, learn_file, index;
	std::string remote, group_string;
	std::string log_file;
	int log_level;

	generic.add_options()
		("help", "This help message")
		("input-dir", bpo::value<std::string>(&input_dir)->required(), "Input directory")
		("learn", bpo::value<std::string>(&learn_file)->required(), "Learning data file")
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
		el.load(index, input_dir, learn_file);

	} catch (const std::exception &e) {
		std::cerr << "exception: " << e.what() << std::endl;
		return -1;
	}
}
