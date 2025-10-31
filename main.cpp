#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <dirent.h>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
// ...existing code...
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
    int rc = std::system("command -v ffmpeg >/dev/null 2>&1");
    if (rc != 0) {
        std::cerr << "Error: 'ffmpeg' not found in PATH. Please install ffmpeg or ensure it's in your PATH.\n";
        return 2;
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
    if (dir_path[0] == '.' && dir_path[1] == '/') {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            dir_path = std::string(cwd) + dir_path.substr(1);
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

    // 并发转换：根据 CPU 核心数启动线程池
    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 1;

    std::queue<std::string> tasks;
    for (const auto& f : video_files) tasks.push(f);

    std::mutex queue_mutex;
    std::condition_variable cv;
    std::atomic<int> remaining(static_cast<int>(video_files.size()));
    std::mutex io_mutex;
    bool stop_flag = false;

    auto worker = [&](){
        while (true) {
            std::string video_file;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                cv.wait(lock, [&]{ return !tasks.empty() || stop_flag; });
                if (tasks.empty()) {
                    if (stop_flag) break;
                    else continue;
                }
                video_file = tasks.front();
                tasks.pop();
            }

            std::string output_file = video_file + "_converted.mp4";
            std::string command = "ffmpeg -y -i \"" + video_file + "\" -vcodec libx264 -acodec aac \"" + output_file + "\" > /dev/null 2>&1";
            {
                std::lock_guard<std::mutex> lg(io_mutex);
                std::cout << "Converting: " << video_file << " to " << output_file << "\n";
            }
            int ret = system(command.c_str());
            {
                std::lock_guard<std::mutex> lg(io_mutex);
                if (ret != 0) {
                    std::cerr << "Error: Conversion failed for " << video_file << "\n";
                } else {
                    remove(video_file.c_str());
                    std::cout << "Successfully converted: " << video_file << "\n";
                }
            }

            if (--remaining == 0) {
                // 最后一个任务完成，通知主线程可以停止 worker
                std::lock_guard<std::mutex> lock(queue_mutex);
                stop_flag = true;
                cv.notify_all();
                break;
            }
        }
    };

    std::vector<std::thread> workers;
    for (unsigned int i = 0; i < num_threads; ++i) {
        workers.emplace_back(worker);
    }

    // 唤醒工作线程开始处理
    cv.notify_all();

    // 等待所有线程结束
    for (auto& t : workers) {
        if (t.joinable()) t.join();
    }

    return 0;
}