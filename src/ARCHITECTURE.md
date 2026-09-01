# 源码模块说明

- `app`：应用程序外壳，负责主窗口、页面导航和各功能模块的装配。
- `widgets`：不包含业务规则的可复用 Qt 控件与布局。
- `features`：按照用户功能划分的控制器、模型和界面代码。
- `domain`：车站、车次、乘车人、订单、车票和余票等业务实体。
- `data`：本地 JSON 数据的读取、保存、异常恢复和默认数据。
- `models`：可供多个功能复用的 Model-View 模型与代理模型。
- `services`：车次查询、购票、余票、退票和统计等业务规则。

计划划分的功能目录包括：

- `query`：车票查询、筛选和排序。
- `passengers`：乘车人信息管理。
- `booking`：购票确认、出票和余票扣减。
- `orders`：订单查询、订单详情和退票。
- `admin`：车站、车次、时刻、票价和余票管理。
- `statistics`：售票、退票、收入和余票统计。
- `settings`：数据目录、手动保存和重新加载。

## 当前依赖方向

```text
界面 / Controller
       ↓
Model / Proxy Model
       ↓
业务 Service
       ↓
DataStore
       ↓
IDataRepository → JsonRepository
```

Controller 只负责收集输入、显示结果和页面跳转。乘车人校验、基础数据约束、
关联删除检查等规则由 Service 执行；Service 通过 `DataStore` 原子提交并自动保存。
除已明确要求的圆角操作按钮等视觉规则外，下拉框、日期框、复选框、表格等优先使用
Qt 原生控件和平台行为，不为原生控件添加额外的 QSS 外观覆盖。

第二阶段已实现 `PassengerTableModel`、`PassengerFilterProxyModel`、
`PassengerService` 和 `AdminService`。第三阶段加入 `QueryService`、
`TrainQueryModel` 和筛选代理；第四阶段加入 `BookingService` 和购票确认流程。
车站、车次、时刻/经停站、席别/票价/余票分别使用独立管理对话框，正式需求未要求的
维护日志不进入当前实现。
