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

#ifndef __WOOKIE_OPERATORS_HPP
#define __WOOKIE_OPERATORS_HPP

#include <elliptics/session.hpp>

#include "index_data.hpp"
#include "storage.hpp"
#include "split.hpp"

#include <algorithm>
#include <condition_variable>

namespace ioremap { namespace wookie {

class find_result {
	public:
		typedef std::function<void (find_result &result, const elliptics::error_info &err)>
			find_completion_callback_t;

		find_result(storage &st, const std::string &text) :
		m_ready(false), m_st(st) {
			m_completion = std::bind(&find_result::on_wait_completion, this,
					std::placeholders::_1, std::placeholders::_2);
			find(text);

			std::unique_lock<std::mutex> guard(m_lock);
			while (!m_ready)
				m_cond.wait(guard);

			m_error.throw_error();
		}

		find_result(storage &st, const std::string &text, const find_completion_callback_t &callback) :
		m_ready(false), m_st(st), m_completion(callback) {
			find(text);
		}

		const std::vector<dnet_raw_id> &results_array() const {
			return m_result_ids;
		}

		const elliptics::sync_find_indexes_result &results_find_indexes_array() const {
			return m_find_result;
		}

		const elliptics::id_to_name_map_t &index_map() const {
			return m_map;
		}

	private:
		bool m_ready;
		storage &m_st;
		wookie::split m_spl;

		find_completion_callback_t m_completion;

		std::condition_variable m_cond;
		std::mutex m_lock;
		elliptics::error_info m_error;
		elliptics::sync_find_indexes_result m_find_result;
		std::vector<dnet_raw_id> m_result_ids;
		elliptics::id_to_name_map_t m_map;

		struct quote {
			std::string 			text;
			std::vector<std::string>	tokens;
			std::vector<dnet_raw_id>	indexes;

			quote(const std::string &txt) : text(txt) {}
		};

		struct find_request {
			std::vector<quote> quotes;
			std::string text;
			std::vector<std::string> text_tokens;
			elliptics::name_to_id_map_t mapper;
		} m_request;

		find_request prepare(const std::string &text) {
			find_request res;
			operators_found scope = std::for_each(text.begin(), text.end(), operators_found());

			res.text = text;
			for (auto op : scope.quotes) {
				res.quotes.emplace_back(text.substr(op.first, op.second - op.first));
				res.text.replace(op.first, op.second - op.first, "");
			}
			return res;
		}

		void prepare_indexes(const std::string &text, std::vector<std::string> &tokens) {
			m_spl.feed(text, tokens);
		}

		struct operators_found {
			int q1, q2, pos;
			std::vector<std::pair<int, int>> quotes;
			std::vector<int> negative;

			operators_found() : q1(-1), q2(-1), pos(-1) {
			}

			operators_found(operators_found &&other) : operators_found() {
				quotes.swap(other.quotes);
				negative.swap(other.negative);
			}

			void operator() (char ch) {
				++pos;

				if (ch == '\"') {
					if (q1 == -1) {
						q1 = pos + 1;
						return;
					}

					q2 = pos;
					quotes.push_back(std::make_pair(q1, q2));

					q1 = q2 = -1;
					return;
				}

				if (ch == '-')
					negative.push_back(pos);
			}
		};

		void find(const std::string &text) {
			using namespace std::placeholders;

			m_request = prepare(text);
			std::vector<std::string> str_indexes;

			// run over all quoted strings
			for (auto qit = m_request.quotes.begin(); qit != m_request.quotes.end(); ++qit) {
				prepare_indexes(qit->text, qit->tokens);
				str_indexes.insert(str_indexes.end(), qit->tokens.begin(), qit->tokens.end());

				qit->indexes = m_st.transform_tokens(qit->tokens);
			}

			// grab tokens from the rest of request (unquoted text)
			prepare_indexes(m_request.text, m_request.text_tokens);
			str_indexes.insert(str_indexes.end(),
					m_request.text_tokens.begin(), m_request.text_tokens.end());

			std::sort(str_indexes.begin(), str_indexes.end());
			str_indexes.erase(std::unique(str_indexes.begin(), str_indexes.end()), str_indexes.end());

			auto session = m_st.create_session();

			for (auto it = str_indexes.begin(); it != str_indexes.end(); ++it) {
				elliptics::key id(*it);
				id.transform(session);

				m_request.mapper[*it] = id.raw_id();
				m_map[id.raw_id()] = *it;
			}

			session.find_all_indexes(str_indexes).connect(
					std::bind(&find_result::on_result_ready, this, _1, _2));
		}

		void on_result_ready(const elliptics::sync_find_indexes_result &result,
				const elliptics::error_info &err) {
			if (err || result.empty()) {
				m_completion(*this, err);
				return;
			}

			m_find_result = result;

			for (auto it = result.begin(); it != result.end(); ++it) {
				const elliptics::find_indexes_result_entry &entry = *it;

				// position to token ID (its pointer) map
				std::map<int, const dnet_raw_id *> pos;

				for (const auto & index : entry.indexes) {
					// unpack index data - this will contain array of
					// token positions in the document
					index_data idata(index.data);

					for (const auto & p : idata.pos)
						pos.insert(std::make_pair(p, &index.index));
				}

				bool all_quotes_ok = true;

				for (auto qit = m_request.quotes.begin(); qit != m_request.quotes.end(); ++qit) {
					size_t state = 0;
					int prev_pos = -1;
					bool quote_ok = false;
					for (auto p : pos) {
#if 0
						std::cout << "state: " << state <<
							", prev-pos: " << prev_pos <<
							", pos: " << p.first <<
							", p-index: " << *p.second <<
							", should-be: " << id_indexes[state] <<
							std::endl;
#endif
						if (state < qit->tokens.size()) {
							if (!memcmp(p.second, &qit->indexes[state],
										sizeof(struct dnet_raw_id))) {
								if (prev_pos == -1) {
									prev_pos = p.first;
									state++;
								} else if (p.first == prev_pos + 1) {
									state++;
									prev_pos++;
								} else {
									prev_pos = -1;
									state = 0;
								}
							} else {
								prev_pos = -1;
								state = 0;

								// check again the first index in quote
								if (!memcmp(p.second, &qit->indexes[state],
										sizeof(struct dnet_raw_id))) {
									prev_pos = p.first;
									state++;
								}
							}
						}

						if (state == qit->tokens.size()) {
							state = 0;
							prev_pos = -1;

							quote_ok = true;
							//std::cout << "good document id: " << entry.id << std::endl;
							break;
						}
					}

					all_quotes_ok &= quote_ok;
				}

				if (all_quotes_ok)
					m_result_ids.push_back(entry.id);
			}

			m_completion(*this, elliptics::error_info());
		}

		void on_wait_completion(find_result &, const elliptics::error_info &err) {
			std::unique_lock<std::mutex> guard(m_lock);
			m_error = err;
			m_ready = true;
			m_cond.notify_all();
		}
};

typedef std::shared_ptr<find_result> shared_find_t;

class operators {
	public:
		operators(storage &st) :
		m_st(st) {
		}

		shared_find_t find(const std::string &text) {
			shared_find_t fobj = std::make_shared<find_result>(m_st, text);
			return fobj;
		}

		shared_find_t find(const std::string &text,
				const find_result::find_completion_callback_t &complete) {
			shared_find_t fobj = std::make_shared<find_result>(m_st, text, complete);
			return fobj;
		}

	private:
		storage &m_st;
};


}}

#endif /* __WOOKIE_OPERATORS_HPP */
