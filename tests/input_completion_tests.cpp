// White-box regression test: compile this file alone (do not also link input.cpp).
#include "../src/core/input.cpp"

#include <cassert>

int main() {
    using namespace arcnova::core;
    const std::vector<std::string> names{"clear", "cls", "cd", "cat"};
    auto result = findCompletions(L"cl", 2, names);
    assert((result.candidates == std::vector<std::wstring>{L"clear", L"cls"}));
    result = findCompletions(L"  CL", 4, names);
    assert(result.candidates.size() == 2);

    const std::wstring middle = L"cat include/core/in tail";
    result = findCompletions(middle, middle.find(L" tail"), names);
    assert(result.start == 4 && result.length == 15);
    assert((result.candidates == std::vector<std::wstring>{L"include/core/input.hpp"}));
    auto replaced = middle;
    replaced.replace(result.start, result.length, result.candidates.front());
    assert(replaced == L"cat include/core/input.hpp tail");

    const std::wstring quoted = L"cat \"include/core/in\" tail";
    result = findCompletions(quoted, quoted.find(L"\" tail"), names);
    assert((result.candidates == std::vector<std::wstring>{L"include/core/input.hpp"}));
    result = findCompletions(L"cd include/co", 13, names);
    assert((result.candidates == std::vector<std::wstring>{L"include/commands/", L"include/core/"}));
    const std::wstring missing = L"ls no-such-dir-1234/a";
    assert(findCompletions(missing, missing.size(), names).candidates.empty());
    std::cout << "PASS: command and path completion regression tests\n";
}
