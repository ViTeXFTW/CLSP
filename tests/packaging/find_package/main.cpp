#include <clsp/Uri.hpp>
#include <string>

int main() {
    std::string encoded = lsp::uri::percentEncode("hello world");
    return encoded == "hello%20world" ? 0 : 1;
}
