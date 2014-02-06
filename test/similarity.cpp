#include "similarity.hpp"

#include <mutex>
#include <thread>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#include <dlib/svm_threaded.h>
#pragma GCC diagnostic pop

using namespace ioremap;
using namespace ioremap::similarity;

class dlib_learner {
	public:
		dlib_learner() {}

		void add_sample(const learn_element &le, int label) {
			if (!le.valid)
				return;

			sample_type s;
			s.set_size(le.features.size());

			for (size_t i = 0; i < le.features.size(); ++i) {
				s(i) = le.features[i];
			}

			m_samples.push_back(s);
			m_labels.push_back(label);
		}

		void train_and_test(const std::string &output) {
			dlib::vector_normalizer<sample_type> normalizer;

			normalizer.train(m_samples);
			for (size_t i = 0; i < m_samples.size(); ++i)
				m_samples[i] = normalizer(m_samples[i]);

			dlib::krr_trainer<kernel_type> trainer;
			trainer.use_classification_loss_for_loo_cv();

			typedef dlib::probabilistic_decision_function<kernel_type> prob_dec_funct_type;
			typedef dlib::normalized_function<prob_dec_funct_type> pfunct_type;

			dlib::randomize_samples(m_samples, m_labels);

			size_t nsize = m_samples.size() / 10;

			std::vector<sample_type> test_samples(std::make_move_iterator(m_samples.begin() + nsize), std::make_move_iterator(m_samples.end()));
			m_samples.erase(m_samples.begin() + nsize, m_samples.end());

			std::vector<double> test_labels(std::make_move_iterator(m_labels.begin() + nsize), std::make_move_iterator(m_labels.end()));
			m_labels.erase(m_labels.begin() + nsize, m_labels.end());

			pfunct_type learned_function;
			learned_function.normalizer = normalizer;
			learned_function.function = dlib::train_probabilistic_decision_function(trainer, m_samples, m_labels, 3);

			std::cout << "\nnumber of basis vectors in our learned_function is " 
				<< learned_function.function.decision_funct.basis_vectors.size() << std::endl;
#if 0
			double max_gamma = 0.003125;

			double max_accuracy = 0;
			for (double gamma = 0.000001; gamma <= 1; gamma *= 5) {
				trainer.set_kernel(kernel_type(gamma));

				std::vector<double> loo_values;
				trainer.train(m_samples, m_labels, loo_values);

				const double classification_accuracy = dlib::mean_sign_agreement(m_labels, loo_values);
				std::cout << "gamma: " << gamma << ": LOO accuracy: " << classification_accuracy << std::endl;

				if (classification_accuracy > max_accuracy)
					max_gamma = gamma;
			}

			trainer.set_kernel(kernel_type(max_gamma));
#endif

			std::ofstream out(output.c_str(), std::ios::binary);
			dlib::serialize(learned_function, out);
			out.close();

			long success, total;
			success = total = 0;

			for (size_t i = 0; i < std::min(test_labels.size(), m_samples.size()); ++i) {
				auto l = learned_function(test_samples[i]);
				if ((l >= 0.5) && (test_labels[i] > 0))
					success++;
				if ((l < 0.5) && (test_labels[i] < 0))
					success++;

				total++;
			}

			printf("success rate: %ld%%\n", success * 100 / total);
		}

	private:
		typedef dlib::matrix<double, 0, 1> sample_type;
		typedef dlib::radial_basis_kernel<sample_type> kernel_type;

		std::vector<sample_type> m_samples;
		std::vector<double> m_labels;
};

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

				int num = sscanf(line.c_str(), "%d\t%d\t", &doc[0], &doc[1]);
				if (num != 2) {
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

					ids.insert(doc[0]);
					ids.insert(doc[1]);

					m_elements.emplace_back(std::move(le));
				}
			}

			m_documents.reserve(ids.size());
			for (auto id : ids) {
				m_documents.emplace_back(document(id));

				m_id_position[id] = m_documents.size() - 1;
			}

			printf("pairs loaded: %zd\n", m_elements.size());
			m_negative_elements.resize(m_elements.size());

			add_documents(std::thread::hardware_concurrency());

			dlib_learner dl;

			for (size_t i = 0; i < m_elements.size(); ++i) {
				dl.add_sample(m_elements[i], +1);
				dl.add_sample(m_negative_elements[i], -1);
			}

			dl.train_and_test(output);
		}

	private:
		std::string m_input;
		std::vector<document> m_documents;
		std::string m_enc_dir;

		std::map<int, int> m_id_position;

		std::vector<learn_element> m_elements;
		std::vector<learn_element> m_negative_elements;

		struct doc_thread {
			int id;
			int step;
		};

		void load_documents(struct doc_thread &dth) {
			document_parser parser;
			parser.load_encodings(m_enc_dir);

			for (size_t i = dth.id; i < m_documents.size(); i += dth.step) {
				document & doc = m_documents[i];

				std::string file = m_input + lexical_cast(doc.id()) + ".html";
				try {
					parser.feed(file.c_str(), "");
					std::string text = parser.text();

					parser.generate_ngrams(text, doc.ngrams());
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

				std::vector<ngram> req_ngrams;
				parser.generate_ngrams(le.request, req_ngrams);

				if (generate_features(le, req_ngrams))
					generate_negative_element(le, m_negative_elements[i], req_ngrams);
			}
		}

		bool generate_features(learn_element &le, const std::vector<ngram> &req_ngrams) {
			int pos1 = m_id_position[le.doc_ids[0]];
			int pos2 = m_id_position[le.doc_ids[1]];

			const std::vector<ngram> &f = m_documents[pos1].ngrams();
			const std::vector<ngram> &s = m_documents[pos2].ngrams();

			if (!f.size() || !s.size())
				return false;

			for (size_t i = 0; i < req_ngrams.size(); ++i) {
				ngram out = ngram::intersect(f[i], s[i]);
				le.features.push_back(out.hashes.size());

				ngram req_out = ngram::intersect(req_ngrams[i], out);
				le.features.push_back(req_out.hashes.size());

			}

			le.features.push_back(req_ngrams[0].hashes.size());
			le.valid = true;
			return true;
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

		void generate_negative_element(learn_element &le, learn_element &negative, const std::vector<ngram> &req_ngrams) {
			int doc_id = le.doc_ids[0];

			negative.doc_ids.push_back(doc_id);

			negative.request = le.request;

			while (1) {
				int pos = rand() % m_documents.size();
				const document &next = m_documents[pos];

				if ((pos == doc_id) || !next.ngrams().size() || (pos == le.doc_ids[1]))
					continue;

				negative.doc_ids.push_back(pos);
				break;
			}

			generate_features(negative, req_ngrams);
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
#if 0
	std::vector<document> docs;

	for (auto f : files) {
		try {
			p.feed(argv[i], 4);
			if (p.hashes().size() > 0)
				docs.emplace_back(argv[i], p.hashes());

#if 0
			std::cout << "================================" << std::endl;
			std::cout << argv[i] << ": hashes: " << p.hashes().size() << std::endl;
			std::ostringstream ss;
			std::vector<std::string> tokens = p.tokens();

			std::copy(tokens.begin(), tokens.end(), std::ostream_iterator<std::string>(ss, " "));
			std::cout << ss.str() << std::endl;
#endif
		} catch (const std::exception &e) {
			std::cerr << argv[i] << ": caught exception: " << e.what() << std::endl;
		}
	}


#endif
}
