# video_converter

## version 1.0
### 说明
video_converter可以将指定目录下的所有视频文件通过ffmpeg转化为libx264:aac格式的视频文件
## version 1.1
### 说明
1. 新增线程池，转换工作将会在线程池中执行
2. 增加对以./开头目录的支持
## 用法
video_converter [dir_path] [options]  
  --help, -h    Show this help message