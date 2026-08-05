#include <shaderc/shaderc.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot open " + path);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

void write_file(const std::string& path, const std::vector<uint32_t>& words) {
  std::ofstream out(path, std::ios::binary);
  if (!out) throw std::runtime_error("cannot write " + path);
  out.write(reinterpret_cast<const char*>(words.data()),
            static_cast<std::streamsize>(words.size() * sizeof(uint32_t)));
}

shaderc_shader_kind kind_from_path(const std::string& path) {
  if (path.ends_with(".vert")) return shaderc_glsl_vertex_shader;
  if (path.ends_with(".frag")) return shaderc_glsl_fragment_shader;
  throw std::runtime_error("unknown shader kind: " + path);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: compile_shaders <input.glsl> <output.spv>\n";
    return 2;
  }
  try {
    const std::string src_path = argv[1];
    const std::string dst_path = argv[2];
    const std::string source = read_file(src_path);

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
    options.SetTargetEnvironment(shaderc_target_env_vulkan,
                                 shaderc_env_version_vulkan_1_1);

    const auto result = compiler.CompileGlslToSpv(
        source, kind_from_path(src_path), src_path.c_str(), options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
      std::cerr << result.GetErrorMessage() << '\n';
      return 1;
    }
    write_file(dst_path, {result.cbegin(), result.cend()});
    std::cout << "wrote " << dst_path << '\n';
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}
