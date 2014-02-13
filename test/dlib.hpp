#ifndef __WOOKIE_DLIB_HPP
#define __WOOKIE_DLIB_HPP

#include "similarity.hpp"

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

		void train_and_test(const std::string &output) {
			dlib::vector_normalizer<sample_type> normalizer;

			normalizer.train(m_samples);
			for (size_t i = 0; i < m_samples.size(); ++i)
				m_samples[i] = normalizer(m_samples[i]);

			dlib::krr_trainer<kernel_type> trainer;
			trainer.be_verbose();
			trainer.use_classification_loss_for_loo_cv();


			dlib::randomize_samples(m_samples, m_labels);

			size_t nsize = m_samples.size() * 9 / 10;

			std::vector<sample_type> test_samples(std::make_move_iterator(m_samples.begin() + nsize), std::make_move_iterator(m_samples.end()));
			m_samples.erase(m_samples.begin() + nsize, m_samples.end());

			std::vector<double> test_labels(std::make_move_iterator(m_labels.begin() + nsize), std::make_move_iterator(m_labels.end()));
			m_labels.erase(m_labels.begin() + nsize, m_labels.end());

			pfunct_type learned_function;
			learned_function.normalizer = normalizer;
			learned_function.function = dlib::train_probabilistic_decision_function(trainer, m_samples, m_labels, 5);

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
		std::vector<sample_type> m_samples;
		std::vector<double> m_labels;
};

}} // namespace ioremap::similarity

#endif /* __WOOKIE_DLIB_HPP */
