#include "dlib.hpp"

#include <mutex>
#include <thread>

using namespace ioremap;
using namespace ioremap::similarity;

class learner {
	public:
		learner(const std::string &input, const std::string &learn_file, const std::string &output, const std::string &enc_dir) : m_input(input), m_enc_dir(enc_dir) {
			srand(time(NULL));

			std::ifstream in(learn_file.c_str());

			std::set<int> ids;
			std::string line;
			int line_num = 0;
			while (std::getline(in, line)) {
				if (!in.good())
					break;

				line_num++;

				int doc[2];
				int label;

				int num = sscanf(line.c_str(), "%d\t%d\t%d\t", &doc[0], &doc[1], &label);
				if (num != 3) {
					fprintf(stderr, "failed to parse string: %d, tokens found: %d\n", line_num, num);
					continue;
				}

				const char *pos = strrchr(line.c_str(), '\t');
				if (!pos) {
					fprintf(stderr, "could not find last tab delimiter\n");
					continue;
				}

				pos++;
				if (pos && *pos) {
					learn_element le;

					le.doc_ids = std::vector<int>(doc, doc+2);
					le.request.assign(pos);

					le.label = -1;
					if (label)
						le.label = +1;

					ids.insert(doc[0]);
					ids.insert(doc[1]);

					m_elements.emplace_back(std::move(le));
				}
			}

			m_documents.reserve(ids.size());
			for (auto id : ids) {
				simdoc tmp;
				tmp.id = id;

				m_documents.emplace_back(tmp);

				m_id_position[id] = m_documents.size() - 1;
			}

			printf("pairs loaded: %zd\n", m_elements.size());

			int num = std::thread::hardware_concurrency();
			if (num == 0)
				num = sysconf(_SC_NPROCESSORS_ONLN);
			add_documents(num);

			dlib_learner dl;
			size_t positive, negative;
			positive = negative = 0;

			for (size_t i = 0; i < m_elements.size(); ++i) {
				const learn_element & le = m_elements[i];

				if (!le.valid)
					continue;

				if (le.label > 0) {
					positive++;
					dl.add_sample(le);
				} else {
					if (negative < positive * 2) {
						negative++;
						dl.add_sample(le);
					}
				}
			}

			dl.train_and_test(output, 0.9);
		}

	private:
		std::string m_input;
		std::vector<simdoc> m_documents;
		std::string m_enc_dir;

		std::map<int, int> m_id_position;

		std::vector<learn_element> m_elements;

		struct doc_thread {
			int id;
			int step;
		};

		void load_documents(struct doc_thread &dth) {
			document_parser parser;
			parser.load_encodings(m_enc_dir);

			for (size_t i = dth.id; i < m_documents.size(); i += dth.step) {
				simdoc & doc = m_documents[i];

				std::string file = m_input + lexical_cast(doc.id) + ".html";
				try {
					if (!parser.feed(file.c_str()))
						continue;

					std::string text = parser.text();

					parser.generate_ngrams(text, doc.ngrams);
				} catch (const std::exception &e) {
					std::cerr << file << ": caught exception: " << e.what() << std::endl;
				}
			}
		}

		void load_elements(struct doc_thread &dth) {
			document_parser parser;
			parser.load_encodings(m_enc_dir);

			for (size_t i = dth.id; i < m_elements.size(); i += dth.step) {
				learn_element &le = m_elements[i];

				int pos1 = m_id_position[le.doc_ids[0]];
				int pos2 = m_id_position[le.doc_ids[1]];

				parser.generate_ngrams(le.request, le.req_ngrams);
				const simdoc &d1 = m_documents[pos1];
				const simdoc &d2 = m_documents[pos2];

				le.generate_features(d1, d2);
			}
		}

		void add_documents(int cpunum) {
			std::vector<std::thread> threads;

			wookie::timer tm;

			for (int i = 0; i < cpunum; ++i) {
				struct doc_thread dth;

				dth.id = i;
				dth.step = cpunum;

				threads.emplace_back(std::bind(&learner::load_documents, this, dth));
			}

			for (int i = 0; i < cpunum; ++i) {
				threads[i].join();
			}

			threads.clear();
			long docs_loading_time = tm.restart();

			for (int i = 0; i < cpunum; ++i) {
				struct doc_thread dth;

				dth.id = i;
				dth.step = cpunum;

				threads.emplace_back(std::bind(&learner::load_elements, this, dth));
			}

			for (int i = 0; i < cpunum; ++i) {
				threads[i].join();
			}

			long elements_loading_time = tm.restart();

			printf("documents: %zd, load-time: %ld msec, elements: %zd, load-time: %ld msec\n",
					m_documents.size(), docs_loading_time, m_elements.size(), elements_loading_time);
		}
};

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("Similarity options");

	std::string mode, input, learn_file, learn_output, encoding_dir;
	generic.add_options()
		("help", "This help message")
		("input", bpo::value<std::string>(&input)->required(), "Input directory")
		("encoding-dir", bpo::value<std::string>(&encoding_dir), "Directory with charset samples")

		("learn", bpo::value<std::string>(&learn_file), "Learning data file")
		("learn-output", bpo::value<std::string>(&learn_output), "Learning output file")
		("mode", bpo::value<std::string>(&mode)->default_value("learn"), "Processing mode: learn/check")
		;

	bpo::variables_map vm;

	try {
		bpo::store(bpo::parse_command_line(argc, argv, generic), vm);
		bpo::notify(vm);
	} catch (const std::exception &e) {
		std::cerr << "Invalid options: " << e.what() << "\n" << generic << std::endl;
		return -1;
	}

	if (!vm.count("input")) {
		std::cerr << "No input directory\n" << generic << std::endl;
		return -1;
	}

	if ((mode == "learn") && (!vm.count("learn") || !vm.count("learn-output"))) {
		std::cerr << "Learning mode requires file with learning data and output serialization file\n" << generic << std::endl;
		return -1;
	}

	if (mode == "learn") {
		learner l(input, learn_file, learn_output, encoding_dir);
		return -1;
	}
}
