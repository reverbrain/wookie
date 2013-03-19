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

		std::cout << ">> " << word << " -> " << s.get(word.data(), word.size()) << std::endl;
	}

	return 0;
}
