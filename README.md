# Qt 列车客运管理系统

暑期学校课程作业，使用 **Qt Widgets / C++17** 实现桌面列车客运管理系统。项目包含本地演示数据、账号权限和购票业务，用于练习桌面界面、业务规则、数据持久化及自动化测试。

## 功能

- **登录与权限**：用户注册、登录和退出；区分管理员与普通用户，普通用户仅管理自己的乘车人和订单。
- **车次查询**：按日期、出发站和到达站查询本地车次，支持筛选和席别选择。
- **乘车人管理**：新增、修改、删除乘车人并校验证件信息。
- **购票与订单**：按日期、区间和席别维护库存，支持购票、订单查询和退票。
- **基础数据管理**：管理员维护车站、车次、经停站、席别、票价和总票额。
- **统计与设置**：统计业务数据，提供本地数据备份和恢复。
- **外部铁路查询**：可选读取 12306 站点与列车信息；外部查询结果只读，与课程演示库存独立，不办理真实购票。

## 构建与测试

需要 Qt 6、CMake 3.16+ 和支持 C++17 的编译器。Qt 组件包括 Widgets、Network、Svg、LinguistTools；运行测试还需要 Test。下面使用 Ninja，也可以选择适合本机的 CMake 生成器。

```bash
git clone https://github.com/miunerofrade/seu-qt-summer-school.git
cd seu-qt-summer-school
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/path/to/Qt/6.x/platform"
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

`CMAKE_PREFIX_PATH` 应指向本机的 Qt 安装目录；已配置 Qt 搜索路径时可以省略该参数。Windows 下应使用与 Qt 安装包匹配的编译器和终端环境。

当前可执行目标名保留为 `untitled`：

- Windows：`build/untitled.exe`。
- macOS：`build/untitled.app`，可用 `open build/untitled.app` 启动。
- Linux：`build/untitled`。

## 本地数据

`QT_SYNC_DATA_DIR` 默认为 `data`，相对路径以可执行文件所在目录为基准。开发时可以将数据目录指定为绝对路径：

```bash
cmake -S . -B build -DQT_SYNC_DATA_DIR="/absolute/path/to/local-data"
```

首次启动会创建本地 `app-data.json` 和内置演示数据。演示管理员为 `admin`，密码为 `admin`；其他用户可在登录页注册。

账号、乘车人、订单、业务备份、登录偏好和铁路查询缓存属于本机运行数据，不提交到仓库。仓库保留公开的 `railway-stations.json` 站点目录。内置乘车人和测试数据为课程演示数据。

课程实现使用明文保存本地账号密码和记住登录信息的功能，请仅使用演示账号，不要输入真实账号密码或真实身份信息。

## 代码结构

```text
src/
├── app/                 # 应用生命周期、主窗口和功能控制器装配
├── features/            # 登录、查询、乘车人、购票、订单、管理、统计和设置
├── domain/              # 业务实体、库存与演示数据
├── data/                # 数据仓库与 JSON 持久化
├── common/              # 公共结果类型和金额表示
└── widgets/             # 公共控件
tests/                   # 登录和各阶段业务测试
data/                    # 公开站点目录及数据使用说明
icons/                   # SVG 图标
```

详细设计见 [架构说明](src/ARCHITECTURE.md)。界面控制器负责交互，业务服务负责规则，数据仓库负责保存；业务通过后才提交持久化结果。

## 外部数据说明

外部铁路数据源仅用于课程中的查询演示。网络或接口格式变化可能导致查询失败，离线时仍可使用内置车站、车次和本地业务流程。本项目不是铁路官方应用。
