#include "dlib.hpp"
#include "elliptics.hpp"
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

			srand(time(NULL));
		}

		void load(const std::string &index, const std::string &train_file, int num) {
			std::vector<std::string> vindexes;
			vindexes.push_back(index);

			elliptics::session session(m_node);
			session.set_groups(m_groups);
			session.set_exceptions_policy(elliptics::session::no_exceptions);
			session.set_ioflags(DNET_IO_FLAGS_CACHE);
			session.set_timeout(600);

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

			for (size_t i = 0; i < m_documents.size(); ++i) {
				const simdoc & doc = m_documents[i];
				m_id_position[doc.id] = i;
			}

			const auto & res = session.read_data(elliptics_element_key(index), 0, 0).get_one().file();

			msgpack::unpacked msg;
			msgpack::unpack(&msg, res.data<char>(), res.size());

			msg.get().convert(&m_elements);
			printf("loaded: keys: %zd, documents: %zd, learn-elements: %zd\n", keys.size(), m_documents.size(), m_elements.size());

			std::ifstream fin(train_file.c_str(), std::ios::binary);
			dlib::deserialize(m_learned_pfunc, fin);

			size_t total = 0;
			size_t success = 0;

			num = std::min<int>(num, m_elements.size());
			for (int i = 0; i < num; ++i) {
				size_t lepos = rand() % m_elements.size();
				learn_element & le = m_elements[lepos];
				learn_element negative;

				if (!make_element(le, negative))
					continue;

				dlib_learner::sample_type s, n;
				s.set_size(le.features.size());
				n.set_size(negative.features.size());

				for (size_t j = 0; j < le.features.size(); ++j)
					s(j) = le.features[j];
				for (size_t j = 0; j < negative.features.size(); ++j)
					n(j) = negative.features[j];

				total += 2;

				auto l = m_learned_pfunc(s);
				if (l >= 0.5) {
					++success;
				} else {
					printf("documents: %d,%d, req: '%s', features: ", le.doc_ids[0], le.doc_ids[1], le.request.c_str());
					for (size_t k = 0; k < le.features.size(); ++k)
						printf("%d ", le.features[k]);
					printf(": %f\n", l);
				}

				l = m_learned_pfunc(n);
				if (l < 0.5) {
					++success;
				} else {
					printf("negative documents: %d,%d, req: '%s', features: ", negative.doc_ids[0], negative.doc_ids[1], negative.request.c_str());
					for (size_t k = 0; k < negative.features.size(); ++k)
						printf("%d ", negative.features[k]);
					printf(": %f\n", l);
				}
			}

			printf("elements-processed: %zd, success rate: %zd%%\n", total, success * 100 / total);
		}

	private:
		std::mutex m_lock;
		std::vector<simdoc> m_documents;
		std::vector<learn_element> m_elements;
		std::map<int, int> m_id_position;

		std::mutex m_cond_lock;
		std::condition_variable m_cond;
		bool m_done;

		elliptics::file_logger m_logger;
		elliptics::node m_node;
		std::vector<int> m_groups;

		dlib_learner::pfunct_type m_learned_pfunc;

		bool make_element(learn_element &le, learn_element &negative) {
			try {
				int pos1 = m_id_position[le.doc_ids[0]];
				int pos2 = m_id_position[le.doc_ids[1]];

				const simdoc &d1 = m_documents[pos1];
				const simdoc &d2 = m_documents[pos2];

				if (!le.generate_features(d1, d2))
					return false;

				generate_negative_element(le, negative);
				return true;
			} catch (const std::exception &e) {
				fprintf(stderr, "learn element check failed: documents: %d,%d, error: %s\n", le.doc_ids[0], le.doc_ids[1], e.what());
				return false;
			}
		}

		void generate_negative_element(learn_element &le, learn_element &negative) {
			negative.doc_ids.push_back(le.doc_ids[0]);

			negative.request = le.request;
			negative.req_ngrams = le.req_ngrams;

			while (1) {
				int pos = rand() % m_documents.size();
				const simdoc &next = m_documents[pos];

				if ((next.id == le.doc_ids[0]) || !next.ngrams.size() || (next.id == le.doc_ids[1]))
					continue;

				negative.doc_ids.push_back(next.id);
				break;
			}

			int pos1 = m_id_position[negative.doc_ids[0]];
			int pos2 = m_id_position[negative.doc_ids[1]];

			const simdoc &d1 = m_documents[pos1];
			const simdoc &d2 = m_documents[pos2];

			negative.generate_features(d1, d2);
		}

		void result_callback(const elliptics::read_result_entry &result) {
			if (result.size()) {
				try {
					msgpack::unpacked msg;
					const elliptics::data_pointer &tmp = result.file();

					msgpack::unpack(&msg, tmp.data<char>(), tmp.size());

					simdoc doc;
					msg.get().convert(&doc);

					std::unique_lock<std::mutex> guard(m_lock);
					//printf("id: %d, text-size: %zd, ngram-size: %zd\n", doc.id, doc.text.size(), doc.ngrams.size());
					m_documents.emplace_back(doc);
				} catch (const std::exception &e) {
					fprintf(stderr, "exception: id: %s, object-size: %zd, error: %s\n",
							dnet_dump_id_str(result.io_attribute()->id), result.size(), e.what());
							
				}
			}
		}

		void final_callback(const elliptics::error_info &error) {
			(void) error;

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
	int num;

	generic.add_options()
		("help", "This help message")
		("model-file", bpo::value<std::string>(&train_file)->required(), "ML model file")
		("index", bpo::value<std::string>(&index)->required(), "Elliptics index for loaded objects")
		("num", bpo::value<int>(&num)->default_value(125), "Number of test samples randomly picked from the list of learn elements")

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
		el.load(index, train_file, num);

	} catch (const std::exception &e) {
		std::cerr << "exception: " << e.what() << std::endl;
		return -1;
	}
}
