#include "include/argparse/include/argparse/argparse.hpp"
#include "cpu.h"


int main(int argc, char *argv[]) {
    argparse::ArgumentParser program("Flip Jump Interpreter");

    program.add_argument("file")
            .help("the flip-jump memory file.")
            .required();

    program.add_argument("-s", "--silent")
            .help("don't show run times")
            .default_value(false)
            .implicit_value(true);

    // TODO support breakpoints (-d, -b, -B).

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        exit(1);
    }

    auto file_path = program.get<std::string>("file");
    auto silent = program.get<bool>("--silent");

    std::ifstream file(file_path);
    if (file.fail()) {
        std::cerr << "Can't open file " << file_path << "." << std::endl;
        exit(1);
    }
    cpu(file, silent);
    return 0;
}