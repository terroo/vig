#include "vig.hpp"

struct Options {
  std::string res = "3x3";
  std::string input_file;
  bool show_help = false;
  bool show_version = false;
};

Options parse_args(int argc, char** argv) {
  Options opt;

  for(int i = 1; i < argc; ++i){
    std::string arg = argv[i];

    if(arg == "--help" || arg == "-h"){
      opt.show_help = true;
    }else if(arg == "--version" || arg == "-v"){
      opt.show_version = true;
    }else if (arg.rfind("--res=", 0) == 0){
      opt.res = arg.substr(6); 
    }else{
      opt.input_file = arg;
    }
  }

  return opt;
}

void print_help() {
  std::cout << "Usage: vig [options] <video_file>\n"
    << "Options:\n"
    << "  --res=WxH        Grid resolution (default: 3x3)\n"
    << "  --help, -h       Show help\n"
    << "  --version, -v    Show version\n";
}

auto main(int argc, char** argv) -> int {

  Options opt = parse_args(argc, argv);

  if(opt.show_help){
    print_help();
    return 0;
  }
  if(opt.show_version){
    std::cout << "vig version 1.0\n";
    return 0;
  }

  if(opt.input_file.empty()){
    std::cerr << "Error: no input file provided.\n";
    return 1;
  }

  int cols, lines;
  size_t x_pos = opt.res.find('x');
  if(x_pos != std::string::npos){
    cols = std::stoi(opt.res.substr(0, x_pos));
    lines = std::stoi(opt.res.substr(x_pos + 1));
  }

  avformat_network_init();
  auto vig = std::make_unique<Vig>(opt.input_file, cols, lines);
  vig->run();
}
