#ifndef __WOOKIE_DLIB_HPP
#define __WOOKIE_DLIB_HPP

#include "similarity.hpp"

#include "wookie/score.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#include <dlib/svm.h>
#pragma GCC diagnostic pop

namespace ioremap { namespace similarity {

class dlib_learner {
	public:
		typedef dlib::matrix<double, 0, 1> sample_type;
		typedef dlib::radial_basis_kernel<sample_type> kernel_type;

		typedef dlib::probabilistic_decision_function<kernel_type> prob_dec_funct_type;
		typedef dlib::normalized_function<prob_dec_funct_type> pfunct_type;

		dlib_learner() {}

		void add_sample(const learn_element &le) {
			if (!le.valid)
				return;

			sample_type s;
			s.set_size(le.features.size());

			for (size_t i = 0; i < le.features.size(); ++i) {
				s(i) = le.features[i];
			}

			m_samples.push_back(s);
			m_labels.push_back(le.label);
		}

		void train_and_test(const std::string &output, double check_part) {
			dlib::vector_normalizer<sample_type> normalizer;

			normalizer.train(m_samples);
			for (size_t i = 0; i < m_samples.size(); ++i)
				m_samples[i] = normalizer(m_samples[i]);

			dlib::krr_trainer<kernel_type> trainer;
			trainer.be_verbose();
			trainer.use_classification_loss_for_loo_cv();


			dlib::randomize_samples(m_samples, m_labels);

			size_t nsize = m_samples.size() * check_part;

			std::vector<sample_type> test_samples(std::make_move_iterator(m_samples.begin() + nsize), std::make_move_iterator(m_samples.end()));
			m_samples.erase(m_samples.begin() + nsize, m_samples.end());

			std::vector<double> test_labels(std::make_move_iterator(m_labels.begin() + nsize), std::make_move_iterator(m_labels.end()));
			m_labels.erase(m_labels.begin() + nsize, m_labels.end());

			pfunct_type learned_function;
			learned_function.normalizer = normalizer;
			learned_function.function = dlib::train_probabilistic_decision_function(trainer, m_samples, m_labels, 3);

			std::cout << "\nnumber of basis vectors in our learned_function is " 
				<< learned_function.function.decision_funct.basis_vectors.size() << std::endl;

			std::ofstream out(output.c_str(), std::ios::binary);
			dlib::serialize(learned_function, out);
			out.close();

			long success, total;
			success = total = 0;

			wookie::score::score score;

			for (size_t i = 0; i < std::min(test_labels.size(), m_samples.size()); ++i) {
				auto l = learned_function(test_samples[i]);

				score.add(test_labels[i], l);

				if ((l >= 0.5) && (test_labels[i] > 0))
					success++;
				if ((l < 0.5) && (test_labels[i] < 0))
					success++;

				total++;
			}

			printf("success rate: %ld%%, precision: %.4f, recall: %.4f, f1: %.4f\n",
					success * 100 / total, score.precision(), score.recall(), score.f1());
		}

	private:
		std::vector<sample_type> m_samples;
		std::vector<double> m_labels;
};

}} // namespace ioremap::similarity

#endif /* __WOOKIE_DLIB_HPP */
