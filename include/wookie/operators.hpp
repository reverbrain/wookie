#ifndef __WOOKIE_OPERATORS_HPP
#define __WOOKIE_OPERATORS_HPP

#include <elliptics/session.hpp>

#include "index_data.hpp"
#include "storage.hpp"
#include "split.hpp"

#include <algorithm>
#include <condition_variable>

namespace ioremap { namespace wookie {

class operators {
	private:
		struct find_request {
			std::vector<std::string> quotes;
			std::vector<std::vector<std::string>> quotes_tokens;
			std::vector<std::vector<dnet_raw_id>> quotes_indexes;
			std::string text;
			std::vector<std::string> text_tokens;
			std::map<std::string, dnet_raw_id> mapper;
		};

		struct find_functor
		{
			find_request request;
			std::function<void (const std::vector<dnet_raw_id> &, const elliptics::error_info &err)> handler;

			void on_result_ready(const elliptics::sync_find_indexes_result &result, const elliptics::error_info &err)
			{
				if (err || result.empty()) {
					handler(std::vector<dnet_raw_id>(), err);
					return;
				}

				std::vector<dnet_raw_id> indexes;
				std::vector<dnet_raw_id> tmp = document_ids(result);
				std::vector<dnet_raw_id> results;

				for (auto it = result.begin(); it != result.end(); ++it) {
					const elliptics::find_indexes_result_entry &entry = *it;

					std::map<int, const dnet_raw_id *> pos;

					for (auto && index : entry.indexes) {
						index_data idata(index.data);

						for (auto p : idata.pos)
							pos.insert(std::make_pair(p, &index.index));
					}

					bool all_quotes_ok = true;

					for (size_t quote_index = 0; quote_index < request.quotes_tokens.size(); ++quote_index) {
						const std::vector<std::string> &indexes_strings = request.quotes_tokens[quote_index];
						const std::vector<dnet_raw_id> &id_indexes = request.quotes_indexes[quote_index];
						indexes.resize(indexes_strings.size());
						tmp.clear();

						for (size_t i = 0; i < indexes.size(); ++i) {
							indexes[i] = request.mapper[indexes_strings[i]];
						}

						size_t state = 0;
						int prev_pos = -1;
						bool quote_ok = false;
						for (auto p : pos) {
#if 0
							std::cout << "state: " << state << ", prev-pos: " << prev_pos << ", pos: " << p.first <<
										 ", p-index: " << *p.second << ", should-be: " << id_indexes[state] << std::endl;
#endif
							if (state < indexes.size()) {
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

							if (state == indexes.size()) {
								state = 0;
								prev_pos = -1;

								tmp.push_back(entry.id);
								quote_ok = true;
								//std::cout << "good document id: " << entry.id << std::endl;
								break;
							}
						}

						all_quotes_ok &= quote_ok;
					}

					if (all_quotes_ok)
						results.push_back(entry.id);
				}

				handler(results, elliptics::error_info());
			}
		};

		struct waiter
		{
			waiter() : ready(false) {}

			bool ready;
			std::condition_variable cond;
			std::mutex lock;
			std::vector<dnet_raw_id> result;
			elliptics::error_info err;

			void on_result(const std::vector<dnet_raw_id> &result, const elliptics::error_info &err) {
				std::unique_lock<std::mutex> guard(lock);
				this->result = result;
				this->err = err;
				cond.notify_all();
			}
		};

	public:
		operators(storage &st) :
		m_st(st) {
		}

		void find(const std::function<void (const std::vector<dnet_raw_id> &, const elliptics::error_info &)> &handler, const std::string &text) {
			using namespace std::placeholders;

			std::shared_ptr<find_functor> functor = std::make_shared<find_functor>();
			functor->handler = handler;

			auto &request = functor->request;
			request = prepare(text);
			std::vector<std::string> indexes;

			for (size_t i = 0; i < request.quotes.size(); ++i) {
				auto new_indexes = prepare_indexes(request.quotes[i], request.quotes_tokens[i]);
				request.quotes_indexes[i] = m_st.transform_tokens(new_indexes);
				indexes.insert(indexes.end(), new_indexes.begin(), new_indexes.end());
			}

			auto new_indexes = prepare_indexes(request.text, request.text_tokens);
			indexes.insert(indexes.end(), new_indexes.begin(), new_indexes.end());

			std::sort(new_indexes.begin(), new_indexes.end());
			new_indexes.erase(std::unique(new_indexes.begin(), new_indexes.end()), new_indexes.end());

			auto session = m_st.create_session();

			for (auto it = new_indexes.begin(); it != new_indexes.end(); ++it) {
				elliptics::key id(*it);
				id.transform(session);

				request.mapper[*it] = id.raw_id();
			}

			session.find_all_indexes(new_indexes).connect(std::bind(&find_functor::on_result_ready, functor, _1, _2));
		}

		std::vector<dnet_raw_id> find(const std::string &text) {
			using namespace std::placeholders;

			waiter w;
			find(std::bind(&waiter::on_result, &w, _1, _2), text);

			std::unique_lock<std::mutex> guard(w.lock);
			while (!w.ready)
				w.cond.wait(guard);

			w.err.throw_error();
			return w.result;
		}

	private:
		storage &m_st;
		wookie::split m_spl;

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

		static std::vector<dnet_raw_id> document_ids(const std::vector<elliptics::find_indexes_result_entry> &objs) {
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


}}

#endif /* __WOOKIE_OPERATORS_HPP */
