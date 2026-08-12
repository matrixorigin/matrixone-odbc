# MatrixOne ODBC / Power BI 深度测试报告

测试日期：2026-08-12

## 结论

当前 ODBC 已具备可用的 Windows x64 开发预览能力：无需客户安装 MySQL
SDK，Unicode/ANSI 驱动、Power BI Import、DirectQuery、公开数据集访问、
基础查询折叠和并发读取均已跑通。

不建议把当前状态声明为稳定生产版。最重要的阻塞项是 MatrixOne
[#26994](https://github.com/matrixorigin/matrixone/issues/26994)：Power Query
折叠出的预处理 `COUNT(?)` 会静默返回错误结果 `2`，而表中实际有 10,000
行。该问题影响结果正确性，优先级高于性能或折叠覆盖率。

仓库已经发布 `v9.7.0-mo.1` Windows x64 预览版。本轮修复应先通过 PR 和
CI，再作为 `v9.7.0-mo.2` 预发布版提供；在 MatrixOne 结果正确性问题、
Gateway、签名和干净机器安装门槛完成前，不升级为稳定版。

## 测试基线

- MatrixOne：`main`，提交
  `9a7c98b8f3aa07fad24b411d54c7a9f6cb3a8731`，本机从源码构建；服务端
  版本字符串为 `8.0.30-MatrixOne-v`。
- MatrixOne ODBC：基于 MySQL Connector/ODBC `9.7.0`；Windows x64
  Unicode 和 ANSI 两个驱动。
- Power BI Desktop：`2.152.1279.0`，64 位。
- Power Query SDK Tools：`2.155.2`。
- Microsoft DataConnectors 测试框架：提交
  `c7b9d81d0d1a62b5f5486f63087c8587e2ca0160`。
- 主机：Windows x64；MatrixOne 运行于 WSL2 Ubuntu 24.04。

## 公开数据集

### TPC-H SF1

使用 MatrixOne 官方 TPC-H 性能测试文档提供的 SF1 数据包，下载包
SHA-256 为
`8e49e487a312e6fbeaad64605fa89e1c92b91cfdd5c92219c274ffa2ed7081e8`。

| 表 | 行数 |
| --- | ---: |
| region | 5 |
| nation | 25 |
| supplier | 10,000 |
| customer | 150,000 |
| part | 200,000 |
| partsupp | 800,000 |
| orders | 1,500,000 |
| lineitem | 6,001,215 |
| 合计 | 8,661,255 |

加载脚本逐表校验行数。MatrixOne 自带的 22 条 TPC-H 查询全部原生执行
成功；随后通过 ODBC 再验证行数、目录元数据、Q1、Q3、Q13、预处理查询、
10 万行类型化读取和 8 线程并发分析。

### Microsoft Power Query SDK 测试数据

使用 Microsoft DataConnectors 仓库中的改造版 NYC Taxi 数据，许可为
CDLA-Permissive-2.0。数据包含 10,000 行行程、10,000 行日期、265 行区域
和 1 行混合类型数据，共 20,266 行。加载脚本固定以下 SHA-256：

| 文件 | SHA-256 |
| --- | --- |
| `nyc_taxi_tripdata.csv` | `CA5389B809077C0CC14B8CE9CCAE5EAC83CB8966F3106F5FA3E95B490DB0B2B8` |
| `nyc_taxi_trip_date_data.csv` | `B220C34567F3236D60DA07E58FCB24EF221C983B9B72B232288D15E99E4EA4DF` |
| `taxi+_zone_lookup.csv` | `598BFDD5170DCD8B9CFB5EC7F8DC283552E1B79CF0BCF3B7A0463FEC1BFEF115` |
| `misc_table.csv` | `05590B86198B12BE1B4B9E29330522AA12AE44479B2B16745E12D8E8293BBF90` |

MySQL 命令行客户端仅用于把公开 CSV 装入 MatrixOne，不是 ODBC、Power BI
连接器或客户安装包的运行时依赖。

## 结果汇总

| 层次 | 结果 | 判定 |
| --- | --- | --- |
| MatrixOne 原生 TPC-H | 22/22 查询通过 | 通过 |
| Unicode ODBC smoke | 全部通过 | 通过 |
| ANSI ODBC smoke | 全部通过 | 通过 |
| Unicode ODBC 深测 | 13 通过 / 5 已知 XFAIL / 0 未知失败 | 通过 |
| ANSI ODBC 深测 | 13 通过 / 5 已知 XFAIL / 0 未知失败 | 通过 |
| Unicode TPC-H ODBC | 8 通过 / 1 已知 XFAIL / 0 未知失败 | 通过 |
| ANSI TPC-H ODBC | 8 通过 / 1 已知 XFAIL / 0 未知失败 | 通过 |
| Power Query Sanity | 8/9 通过 | 有 1 个官方基线差异 |
| Power Query Standard 功能 | 188/203 通过 | 15 个差异已分类 |
| Power Query Standard 严格折叠 | 至少 115/203 完全折叠 | 预览级覆盖 |
| 上游 Connector/ODBC 差分 | 313 TAP 断言通过 / 117 失败 | 诊断数据，不作为 MO 发布门槛 |
| Power BI Desktop Import | 真实应用读取 5 行混合类型数据 | 通过 |
| Power BI Desktop DirectQuery | 聚合、Unicode、负数、刷新 | 通过 |
| MSI 管理解包与 DLL 装载 | 31 个运行时文件，3 个核心 DLL 均可装载 | 通过 |

### ODBC 专项覆盖

18 项深测覆盖连接能力、目录 API、结果描述符、20 类数据、Unicode、
二进制、JSON、NULL、预处理参数、浮点和布尔、Power BI 形状的嵌套查询、
聚合、HAVING、LIMIT/OFFSET、事务、隔离级别、64 KiB 流式参数、分块读取、
SQLSTATE、6 连接 72 次并发读取、查询超时和取消。

TPC-H ODBC 测试在两个驱动上分别验证：

- 8 张表行数和公开目录元数据；
- Q1 单表聚合、Q3 三表连接、Q13 外连接和嵌套查询；
- Power Query 常见的预处理折叠 SQL；
- 10 万行类型化读取，耗时约 0.66 至 0.75 秒；
- 8 线程、每线程 5 次分析查询，整组约 0.5 秒。

该并发结果只证明基本线程安全和本机可用性，不是正式吞吐量基准。

### Power Query SDK 结果解释

Sanity 的唯一差异是 Microsoft 预期输出把一个 DDL 中声明为 `NOT NULL`
的日期列标成可空，而 MatrixOne ODBC 如实报告不可空。这是测试基线差异，
不是驱动错误。

Standard 功能模式的 15 个差异已经按直接 SQL、ODBC 和 Mashup 本地回退
逐项缩小：

- `FoldListCount` 和 `FoldTableRowCount` 暴露 MatrixOne #26994；
- 无 scale 的 `CAST(... AS DECIMAL)` 与 Microsoft 预期的小数语义不同；
- `RoundingMode.Down`、`WeekOfYear` 和日期 duration 在当前 Mashup SDK
  版本的本地回退结果与仓库基线不同；
- 字符转数字和负长度 `Text.Middle` 的直接 ODBC SQL结果正确，但 PQTest
  抛出 `NullReferenceException`。

严格 `--failOnFoldingFailure` 模式共 88 个未通过，其中 57 个是 PQTest
自身的 `NullReferenceException`，20 个明确未完全折叠，11 个输出差异。
因此 115/203 是保守下限，不能把全部 88 项都归因于 ODBC
或 MatrixOne。未完全折叠主要集中在数学、日期时间、类型转换、日期、文本
和函数类表达式。

### 真实 Power BI Desktop

Import 模式成功导入 5 行、7 列混合类型 fixture。DirectQuery 模式实际向
MatrixOne 发出分组 `SUM` 查询，返回：`A=30`、`B=30`、`Power BI=-7.25`、
`上海=1234.50`，总计 `1287.25`。

在重新打开 PBIX 并点击刷新后，MatrixOne 记录到新的 `SHOW KEYS` 和分组
聚合 SQL，画布总计保持 `1287.25`。这证明不是只保存了旧缓存或只完成连接
对话框，而是 Desktop 的真实 DirectQuery 刷新路径可用。

## 本轮 ODBC 修复

1. `SQLColumns` 过滤 MatrixOne 泄露的 `__mo_fake_pk_col` 和
   `__mo_cpkey_col`，避免 Power BI Navigator 把物理辅助列加入模型。
2. MatrixOne `BOOL` 在 `SQLColumns` 和普通结果描述符中统一映射为
   ODBC `SQL_BIT`；普通 `TINYINT` 仍保持 `SQL_TINYINT`。
3. 放开 `SQL_BIT` 到标准 ODBC 数值 C 类型的正常转换，避免修正类型后
   产生 `07006` 回归。
4. 新增无主键表隐藏列、BOOL/TINYINT 区分、TPC-H 目录元数据和公开数据
   回归测试。

这些兼容层修复有自动化断言；MatrixOne 元数据根因仍由 #26993 跟踪。

## MatrixOne 问题清单

| 优先级 | Issue | Power BI / ODBC 影响 | 建议 |
| --- | --- | --- | --- |
| P0 | [#26994](https://github.com/matrixorigin/matrixone/issues/26994) | 预处理 `COUNT(?)` 静默返回 2，属于结果错误 | 稳定版硬阻塞，优先修复并加入 COM_STMT 回归 |
| P0 | [#26967](https://github.com/matrixorigin/matrixone/issues/26967) | utf8mb4 VARCHAR 描述长度按 3 字节计算，ODBC `ColumnSize` 变小 | 修复协议字段长度，避免 BI 模型长度错误 |
| P1 | [#26993](https://github.com/matrixorigin/matrixone/issues/26993) | `information_schema.columns` 泄露复合主键和无主键物理列 | ODBC 已绕过，MO 元数据层仍应修复 |
| P1 | [#26769](https://github.com/matrixorigin/matrixone/issues/26769) | 不支持无 LIMIT 的 OFFSET，限制 Power Query 分页折叠 | 补齐兼容语法后再扩大 connector 能力声明 |
| P1 | [#26716](https://github.com/matrixorigin/matrixone/issues/26716) | VARBINARY 缺少 `BINARY_FLAG`，二进制转文本失败 | 修复协议元数据 |
| P1 | [#26678](https://github.com/matrixorigin/matrixone/issues/26678) | `max_execution_time` 不生效，ODBC 超时不能终止查询 | Gateway 和共享集群上线前修复 |
| P2 | [#26715](https://github.com/matrixorigin/matrixone/issues/26715) | 未加反引号的合法 Unicode 标识符仍被拒绝 | 已在最新 main 复现并重新打开 |

## 安装包与版本建议

客户不需要 MySQL SDK。MSI/ZIP 必须携带 ODBC DLL、`libmysql.dll`、OpenSSL、
Kerberos/SASL 运行库、认证插件和 `MatrixOne.mez`；SDK 中的头文件、静态库
和 PDB 不进入普通运行包。

本轮本地 `9.7.0-mo.2` MSI 候选已完成管理安装解包，SHA-256 为
`2afd89e991ab5173150c72a2669db5922c4ea5687b85f1dbfcaee3d933154288`。
解包后的 Unicode、ANSI 和 setup DLL 均能只依靠随包目录成功
`LoadLibrary`，并且运行目录中没有 `.h`、`.lib` 或 `.pdb`。该候选未签名，
尚未发布。

建议继续使用 GitHub Releases 交付原生安装资产，而不是创建 NuGet 等
GitHub Packages 包。每个预发布版至少提供：

- Windows x64 MSI；
- 便携 ZIP；
- 单独的 `MatrixOne.mez`；
- `SHA256SUMS.txt`；
- 中文测试报告和已知问题；
- 可选的独立 symbols ZIP，不混入客户运行包。

`v9.7.0-mo.2` 的发布条件是本 PR 合并、CI 通过、使用合并提交重建全部
资产并重新执行 smoke。稳定版还需要：修复 #26994、完成干净 Windows
机器的安装/升级/卸载、Power BI on-premises gateway 刷新、代码和连接器
签名、TLS 证书校验以及更完整的严格折叠覆盖。

## 可复现入口

- `test/mo_odbc_smoke.c`：基础 ODBC API。
- `test/mo_odbc_deep.cc`：18 项 Unicode/ANSI 深测。
- `test/mo_odbc_tpch.cc`：TPC-H SF1 ODBC 覆盖。
- `test/tpch/load_tpch.ps1`：TPC-H 建表、加载和行数校验。
- `test/tpch/run_tpch_queries.sh`：MatrixOne 原生 22 查询。
- `test/powerquery/load_powerquery.ps1`：公开 Power Query 数据装载和 hash
  校验。
- `test/powerquery/run_powerquery_tests.ps1`：固定 Microsoft 框架提交并运行
  功能或严格折叠测试。

## 尚未覆盖

- Power BI Service / on-premises data gateway 的计划刷新；
- 干净 Windows VM 上的 MSI 交互安装、修复、升级和卸载；
- 签名后的 Power BI 受信任扩展策略；
- TLS CA、主机名、过期证书和生产认证组合；
- 长时间 soak、高连接数、故障注入和正式性能基准；
- DirectQuery 所有 DAX 到 M/SQL 的转换组合。

因此“客户下载后基本可连接、导入、DirectQuery 和刷新”已有真实证据，
但不能据此承诺所有 Power BI 功能和生产环境已经全覆盖。
