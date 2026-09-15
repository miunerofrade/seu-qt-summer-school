# [redacted]_Course Contributor 2

Qt 6 / C++17 编写的桌面应用。

## 运行发布版

解压 Windows 发布包后，双击 `app/untitled.exe`。请保留 `app` 目录中的 DLL、
Qt 插件目录及 `data` 目录的相对位置；程序会从可执行文件旁的 `data` 目录读取和
写入本地 JSON 数据。

## 从源码构建

需要 Qt 6（Widgets、Network、Svg、Test）、CMake 和支持 C++17 的编译器。

```powershell
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure
```

`QT_SYNC_DATA_DIR` 默认为 `data`，相对路径会以可执行文件所在目录为基准解析；
开发时也可以在 CMake 配置阶段传入绝对数据目录。
