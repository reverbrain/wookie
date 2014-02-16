/*
 * Copyright 2013+ Evgeniy Polyakov <zbr@ioremap.net>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __WOOKIE_F1_HPP
#define __WOOKIE_F1_HPP

namespace ioremap { namespace wookie { namespace score {

struct cnt {
	size_t value;

	size_t add(ssize_t num) {
		value += num;
		return value;
	}

	cnt() : value(0) {}
};

class score {
	public:
		score() {}

		size_t add_true_positive(ssize_t num) {
			return m_true_positive.add(num);
		}

		size_t add_true_negative(ssize_t num) {
			return m_true_negative.add(num);
		}

		size_t add_false_positive(ssize_t num) {
			return m_false_positive.add(num);
		}

		size_t add_false_negative(ssize_t num) {
			return m_false_negative.add(num);
		}

		double precision() {
			return (double)m_true_positive.value / (double)(m_true_positive.value + m_false_positive.value);
		}

		double recall() {
			return (double)m_true_positive.value / (double)(m_true_positive.value + m_false_negative.value);
		}

		double f1() {
			return 2.0 * precision() * recall() / (precision() + recall());
		}

		void add(bool value_positive, bool measured_positive) {
			if (measured_positive) {
				if (value_positive)
					add_true_positive(1);
				else
					add_false_positive(1);
			} else {
				if (!value_positive)
					add_true_negative(1);
				else
					add_false_negative(1);
			}
		}

	private:
		cnt m_true_positive, m_false_positive;
		cnt m_true_negative, m_false_negative;
};

}}} // namespace ioremap::wookie::score

#endif /* __WOOKIE_F1_HPP */
