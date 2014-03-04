#ifndef __WOOKIE_SIMILARITY_HPP
#define __WOOKIE_SIMILARITY_HPP

#include "simdoc.hpp"

#include "wookie/tfidf.hpp"
#include "wookie/timer.hpp"
#include "wookie/url.hpp"

#include <algorithm>
#include <fstream>
#include <list>
#include <sstream>
#include <vector>

#include <boost/program_options.hpp>

#include <msgpack.hpp>

namespace ioremap { namespace similarity {

static inline std::vector<wookie::lngram> intersect(const std::vector<wookie::lngram> &f, const std::vector<wookie::lngram> &s)
{
	std::vector<wookie::lngram> tmp;

	if (!f.size() || !s.size())
		return tmp;

	tmp.resize(std::min(f.size(), s.size()));
	auto inter = std::set_intersection(f.begin(), f.end(), s.begin(), s.end(), tmp.begin());
	tmp.resize(inter - tmp.begin());

	return tmp;
}

struct learn_element {
	learn_element() : label(-1), valid(false) {
	}

	std::vector<int> doc_ids;
	std::string request;
	std::vector<wookie::lngram> req_ngrams;

	int label;
	bool valid;

	std::vector<int> features;

	MSGPACK_DEFINE(doc_ids, label, request, req_ngrams);

	bool generate_features(const simdoc &d1, const simdoc &d2) {
		const std::vector<wookie::lngram> &f = d1.ngrams;
		const std::vector<wookie::lngram> &s = d2.ngrams;

		if (!f.size() || !s.size())
			return false;

		features.push_back(f.size());
		features.push_back(s.size());

		std::vector<wookie::lngram> inter = intersect(f, s);
		features.push_back(inter.size());

		features.push_back(req_ngrams.size());
		features.push_back(intersect(inter, req_ngrams).size());

		valid = true;
		return true;
	}
};

}} // namespace ioremap::similarity

#endif /* __WOOKIE_SIMILARITY_HPP */
