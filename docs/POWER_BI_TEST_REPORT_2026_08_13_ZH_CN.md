# Power BI → MatrixOne ODBC → MatrixOne main 深度测试报告

测试日期：2026-08-13

## 结论

本轮基于 MatrixOne `main` 提交
`2d9ee9c75398b5492f14db7d9bd4d9f777251bd3`、Power BI Desktop
`2.156.951.0` 和 MatrixOne ODBC `main` 执行公开测试与 10 个故障/恢复大场景。

ODBC 已修复 3 类客户可见问题：

1. Power Query 生成的 `{fn ...}` ODBC 标量函数没有转换，导致 MatrixOne
   SQL 解析失败；
2. Windows `ODBC_CONFIG_DSN` 更新会覆盖新属性并可能删除 DSN；
3. MatrixOne 重启后，Power BI DirectQuery 会持续复用失效连接，必须重启
   Desktop 才能恢复。

修复后，公开 Power Query 功能测试为 **206/212**，专用 ODBC 深测为
**29 PASS / 8 个已登记 MatrixOne XFAIL / 0 FAIL**，10 个自设计大场景全部
完成失败注入和恢复验证。建议合并本轮 PR 后发布新的开发预览版，不建议
在 MatrixOne P0/P1 正确性问题关闭前声明为稳定生产版。

## 测试基线与公开数据

- MatrixOne：`main`，`2d9ee9c75398b5492f14db7d9bd4d9f777251bd3`；
- MatrixOne ODBC：`main` 基线 `590cf65f3d4aafb4f75fac1d042f14aed073a9bd`；
- Power BI Desktop：`2.156.951.0`，64 位；
- Power Query SDK Tools：`2.155.2`；
- Microsoft DataConnectors：
  `c7b9d81d0d1a62b5f5486f63087c8587e2ca0160`；
- 公开 NYC Taxi 派生数据：10,000 行行程、10,000 行日期、265 行区域和
  1 行混合类型数据；
- 既有 TPC-H SF1：8 表、8,661,255 行；
- NESR：百万行传感器明细和多张报表金表。

公开 CSV 的加载工具不是客户运行依赖。客户安装包仍只需要随包运行库，
不需要 MySQL SDK、头文件或静态库。

## 公开测试结果

| 测试 | 结果 | 解释 |
| --- | ---: | --- |
| Microsoft Power Query SDK 功能 | 206/212 | 4 个 ODBC escape 失败已修复；剩余 6 个为官方期望/本地 M 行为差异 |
| Power Query 严格折叠 | Sanity 8/9；Standard 21/203 | 关闭参数绑定后，官方诊断期望仍要求 `?`，不能与功能正确率混算 |
| pyodbc 官方测试 | 31 PASS / 7 测试环境失败 | 7 项在进入 ODBC 前依赖 wheel 私有测试接口 |
| Connector/ODBC 上游 35 程序 | 332 TAP PASS / 131 FAIL | 包含大量 MySQL Server 专属语法/插件；另有 1 个环境策略阻止启动 |
| MatrixOne ODBC smoke | PASS | 目录、类型、Unicode、参数和宽字符 API |
| MatrixOne ODBC deep | 29 PASS / 8 XFAIL / 0 FAIL | 增加 ODBC 标量函数 escape 回归，总计 37 项 |
| Windows setup | 3/3 | DSN 新建、非交互更新、删除 |

Power Query 剩余 6 个差异是：Sanity 的 `DateTypeSchema`，4 个
`RoundingMode.Down`，以及 `DateAddDuration`。诊断显示这些值在 M 引擎本地
计算；没有再把错误 `{fn ...}` SQL 发给 MatrixOne。严格折叠数字低的主要
原因是为规避 MatrixOne #27034，连接器暂时关闭参数绑定，官方 `.pqout`
仍把参数化命令文本作为完全折叠条件。功能正确和完全折叠必须分别报告。

## 自设计 10 个大场景

| # | 大场景 | 结果与关键证据 |
| ---: | --- | --- |
| 1 | NESR 百万行 Import、旧 WSL 地址漂移与 DSN 恢复 | 先得到 08001，改为 `localhost:6001` 后不重启报表即可刷新；ODBC 校验 10/10 |
| 2 | DirectQuery 聚合和真实视觉对象 | Power BI 实际下推 `GROUP BY category, SUM(amount)`，中文、负数和小数正确 |
| 3 | NYC Taxi 公开数据复杂连接/筛选/聚合/排序 | 10,000 行与 265 区域表连接，返回 5 个 Borough；Manhattan 5,817 行 |
| 4 | Unicode、DECIMAL、DATE、DATETIME(6)、BOOL、NULL | 中文参数往返、`1234.50`、微秒和 NULL 全部保持 |
| 5 | catalog/schema/table/view/column 导航 | `SQLTables`/`SQLColumns` 与 Power Query Navigator 路径通过 |
| 6 | MO 服务中断及同 Power BI 进程恢复 | 停机返回 08S01；关闭连接池后 MO 恢复 2 秒，同进程恢复 7 列 6 行 |
| 7 | 两个 Power BI 并发刷新与 64 路 ODBC 查询 | Import、DirectQuery 同时刷新；64 路查询全部成功 |
| 8 | 错端口、错库、错密码及恢复 | 依次得到 08001、HY000/1049、28000/1045，随后 `SELECT 1` 成功 |
| 9 | Import 与 DirectQuery 数据新鲜度 | 插入第 6 行；DirectQuery 立即看到 6 行，Import 从 5 行刷新到 6 行 |
| 10 | 删除模型依赖列并原地恢复 | Power BI 得到 HY000/20301；恢复列后同一报表恢复 7 列 6 行 |

## 本轮 ODBC 修复

### ODBC 标量函数 escape

驱动仅在连接到 MatrixOne 时，把 `{fn function(...)}` 的包装转换成
MatrixOne 可解析的函数调用，同时保留日期/时间 escape、字符串、标识符和
注释内的花括号。自动化覆盖 ASCII、负长度 SUBSTRING、嵌套函数、函数内
嵌套日期 escape 和引号内 `{fn ...}` 文本。

此修复让公开用例中的 `CharacterToNumber`、负长度 `Text.Middle` 和两个
`WeekOfYear` 从失败变为通过，并恢复大量数学、日期、时间和文本函数的
服务端折叠能力。

### Power BI 连接器

- 暂时关闭参数绑定，规避 MatrixOne #27034 导致的静默错误聚合；
- 禁止把无 scale 的 DECIMAL/NUMERIC 转换错误下推；
- 关闭 `ClientConnectionPooling`，使 MatrixOne 重启后同一 Power BI
  DirectQuery 进程能建立新物理连接并恢复。

### Windows DSN 配置

`ODBC_CONFIG_DSN` 现在会用调用者的新属性覆盖旧 DSN，并把注册表中的 DLL
路径解析回注册驱动名。PowerShell `Set-OdbcDsn` 和 C setup 测试均验证
端口/数据库更新后 DSN 不会丢失。

## MatrixOne issue 清单

| 优先级 | Issue | 影响 |
| --- | --- | --- |
| P0 | [#27034](https://github.com/matrixorigin/matrixone/issues/27034) | 参数数组 SELECT 分支映射错误；Power Query `COUNT(?)` 对 10,000 行静默返回 2。本轮已补充 Power BI 证据 |
| P0 | [#26967](https://github.com/matrixorigin/matrixone/issues/26967) | utf8mb4 VARCHAR 长度元数据缩小，影响模型 ColumnSize |
| P1 | [#26769](https://github.com/matrixorigin/matrixone/issues/26769) | 不支持无 LIMIT 的 OFFSET，限制 DirectQuery 分页折叠 |
| P1 | [#27035](https://github.com/matrixorigin/matrixone/issues/27035) | `sql_select_limit` 被忽略，`SQL_ATTR_MAX_ROWS` 不生效 |
| P1 | [#27024](https://github.com/matrixorigin/matrixone/issues/27024) | 缺列返回 HY000/20301 而不是 42S22；模式漂移诊断不精确 |
| P1 | [#26678](https://github.com/matrixorigin/matrixone/issues/26678) | `max_execution_time` 不执行，ODBC 查询超时不可靠 |
| P2 | [#27036](https://github.com/matrixorigin/matrixone/issues/27036) | PAD_CHAR_TO_FULL_LENGTH 被接受但 CHAR 不补齐 |
| P2 | [#26715](https://github.com/matrixorigin/matrixone/issues/26715) | 未加引号的 Unicode 标识符被拒绝 |

本轮没有为同一根因重复创建 MatrixOne issue。新增 Power BI 影响已作为评论
补充到 #27034；模式漂移稳定复现归入已有 #27024。

## 发布建议

合并本轮 PR 和 CI 通过后，可以发布一个新的 Windows x64 **developer
preview / prerelease**，建议资产包括 MSI、便携 ZIP、单独 `MatrixOne.mez`、
`SHA256SUMS.txt`、中文报告和已知问题。ODBC 原生驱动不适合用 NuGet 代替
安装包；GitHub Release assets 是当前最直接的客户交付方式。

稳定版仍应以 #27034、#26967、#27024、#26678 的修复，以及干净 Windows
VM 安装/升级/卸载、Gateway、TLS 和签名测试为发布门槛。
