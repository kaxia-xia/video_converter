#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <dirent.h>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Error: No directory path provided.\n";
        std::cerr << "Use --help or -h for usage information.\n";
        return 1;
    }
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Convert video files in the specified directory to libx264/aac by ffmpeg.\n";
            std::cout << "Usage: video_converter [dir_path] [options]\n";
            std::cout << "  --help, -h    Show this help message\n";
            return 0;
        }
    }
    DIR* dir;
    struct dirent* entry;
    std::string dir_path = argv[1];
    for (auto& ch : dir_path) {
        if (ch == '\\') {
            ch = '/';
        }
    }
    if (dir_path.back() != '/') {
        dir_path += '/';
    }
    if (dir_path[0] == '~') {
        const char* home = getenv("HOME");
        if (home) {
            dir_path = std::string(home) + dir_path.substr(1);
        }
    }
    //将目录中的.转换为当前目录
    if (dir_path == "./") {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            dir_path = std::string(cwd) + '/';
        }
    }
    std::vector<std::string> video_files;
    std::queue<std::string> dir_queue;
    dir_queue.push(dir_path);
    while (!dir_queue.empty()) {
        std::string current_dir = dir_queue.front();
        dir_queue.pop();
        dir = opendir(current_dir.c_str());
        if (dir == nullptr) {
            std::cerr << "Error: Could not open directory " << current_dir << "\n";
            continue;
        }
        while ((entry = readdir(dir)) != nullptr) {
            std::string file_name = entry->d_name;
            if (file_name == "." || file_name == "..") {
                continue;
            }
            std::string full_path = current_dir + file_name;
            if (entry->d_type == DT_DIR) {
                dir_queue.push(full_path + '/');
            } else {
                video_files.push_back(full_path);
            }
        }
        closedir(dir);
    }
    for (const auto& video_file : video_files) {
        std::string output_file = video_file + "_converted.mp4";
        std::string command = "ffmpeg -i \"" + video_file + "\" -vcodec libx264 -acodec aac \"" + output_file + "\" > /dev/null 2>&1";
        std::cout << "Converting: " << video_file << " to " << output_file << "\n";
        int ret = system(command.c_str());
        if (ret != 0) {
            std::cerr << "Error: Conversion failed for " << video_file << "\n";
        } else {
            remove(video_file.c_str());
            std::cout << "Successfully converted: " << video_file << "\n";
        }
    }
    return 0;
}