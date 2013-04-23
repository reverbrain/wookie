#ifndef __WOOKIE_OPERATORS_HPP
#define __WOOKIE_OPERATORS_HPP

#include "wookie/index_data.hpp"

#include "elliptics/session.hpp"

#include <algorithm>

namespace ioremap { namespace wookie {

class operators {
	public:
		operators(storage &st) :
		m_st(st) {
		}

		std::vector<dnet_raw_id> find(const std::string &text) {
			operators_found scope = std::for_each(text.begin(), text.end(), operators_found());
			std::vector<dnet_raw_id> results;

			std::string nt(text);
			for (auto op : scope.quotes) {
				std::string quote = text.substr(op.first, op.second - op.first);
				nt.replace(op.first, op.second - op.first, "");

				if (results.size() == 0)
					results = find_quoted_chunk(quote);
				else
					results = intersect(results, find_quoted_chunk(quote));

			}
			if (results.size() == 0)
				results = find_chunk(nt);
			else
				results = intersect(results, find_chunk(nt));
			return results;
		}

	private:
		storage &m_st;
		wookie::split m_spl;

		std::vector<dnet_raw_id> intersect(std::vector<dnet_raw_id> &results, const std::vector<dnet_raw_id> &tmp) {
			std::vector<dnet_raw_id> ret;
			std::set_intersection(results.begin(), results.end(), tmp.begin(), tmp.end(),
					std::back_inserter(ret),
					ioremap::elliptics::dnet_raw_id_less_than<ioremap::elliptics::skip_data>());

			return std::move(ret);
		}

		std::vector<dnet_raw_id> document_ids(const std::vector<elliptics::find_indexes_result_entry> &objs) {
			std::vector<dnet_raw_id> results;

			for (auto && entry : objs) {
				//std::cout << "document id: " << entry.id << std::endl;
				results.emplace_back(entry.id);
			}

			return std::move(results);
		}

		std::vector<std::string> prepare_indexes(const std::string &text, std::vector<std::string> &tokens) {
			wookie::mpos_t pos = m_spl.feed(text, tokens);

			std::vector<std::string> indexes;
			std::transform(pos.begin(), pos.end(), std::back_inserter(indexes),
					std::bind(&mpos_t::value_type::first, std::placeholders::_1));

			return std::move(indexes);
		}

		std::vector<dnet_raw_id> find_chunk(const std::string &text) {
			std::vector<std::string> tokens;
			return std::move(document_ids(m_st.find(prepare_indexes(text, tokens))));
		}

		std::vector<dnet_raw_id> find_quoted_chunk(const std::string &text) {
			std::vector<std::string> tokens;
			std::vector<std::string> indexes = prepare_indexes(text, tokens);

			std::vector<dnet_raw_id> id_indexes;
			id_indexes = m_st.transform_tokens(tokens);

			std::vector<elliptics::find_indexes_result_entry> objs = m_st.find(id_indexes);

			std::vector<dnet_raw_id> results;
			for (auto && entry : objs) {
				std::map<int, dnet_raw_id *> pos;

				for (auto && index : entry.indexes) {
					index_data idata(index.second);

					for (auto p : idata.pos)
						pos.insert(std::make_pair(p, &index.first));
				}

				int state = 0;
				int prev_pos = -1;
				for (auto p : pos) {
#if 0
					std::cout << "state: " << state << ", prev-pos: " << prev_pos << ", pos: " << p.first <<
						", p-index: " << *p.second << ", should-be: " << id_indexes[state] << std::endl;
#endif
					if (state < (int)indexes.size()) {
						if (!memcmp(p.second, &id_indexes[state], sizeof(struct dnet_raw_id))) {
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

							if (!memcmp(p.second, &id_indexes[state], sizeof(struct dnet_raw_id))) {
								prev_pos = p.first;
								state++;
							}
						}
					}

					if (state == (int)indexes.size()) {
						state = 0;
						prev_pos = -1;

						results.push_back(entry.id);
						//std::cout << "good document id: " << entry.id << std::endl;
						break;
					}
				}
			}

			return std::move(results);
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
};


}};

#endif /* __WOOKIE_OPERATORS_HPP */
