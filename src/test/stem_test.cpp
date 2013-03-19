#include <stem/stem.hpp>

using namespace ioremap::wookie;

int main(int argc, char *argv[])
{
	char *lang = (char *)"rus";

	if (argc != 1)
		lang = argv[1];

	stem s(lang, NULL);

	while (true) {
		std::string word;

		std::cin >> word;

		std::cout << ">> " << word;
		while (true) {
			std::string tmp = s.get(word.data(), word.size());

			if (tmp == word)
				break;

			word = tmp;
			std::cout << " -> " << tmp;
		}
		std::cout << std::endl;
	}

	return 0;
}
