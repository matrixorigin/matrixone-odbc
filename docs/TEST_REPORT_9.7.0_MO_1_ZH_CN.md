# MatrixOne ODBC 9.7.0-mo.1 中文测试报告

测试日期：2026-08-11（Asia/Shanghai）
报告修订日期：2026-08-12

## 1. 结论

本版本已经验证 Windows x64 ODBC 驱动的核心连接、元数据、数据读写和
Power BI Desktop 的最小 Import/DirectQuery 端到端链路。它可以作为开发者
预览版用于联调和问题定位，但**还不能据此声明已经达到客户生产环境的完整
Power BI 兼容性**。

当前 Power BI 实测使用的是 5 行自建专项数据，不是公开数据集。它适合验证
中文、数值、日期时间、布尔值、空值和基本聚合，但不能代表多表模型、大数据量、
复杂 Power Query 转换或 Power BI Service/Gateway 场景。

## 2. 源码和环境

- MatrixOne ODBC：`0338422922cdf2c2ddd0d42c72066125faccafab`
  （`codex/windows-powerbi-validation`，已合并到 `main`）
- MatrixOne：`0f145bbc0a020e979e5bc2515de1a8a21ae4f222`（`main`）
- MatrixOne 协议版本：`8.0.30-MatrixOne-v`
- Power BI Desktop：`2.152.1279.0` x64
- ODBC 驱动版本：`09.07.0000`，Windows x64 RelWithDebInfo
- 测试地址：`localhost:6001`

MySQL 9.7.1 SDK 只用于编译阶段的头文件和导入库。MSI 和 ZIP 已携带运行所需
DLL 和认证插件，客户不需要下载 MySQL SDK，也不需要安装 MySQL Server。

## 3. 测试数据说明

本轮没有使用公开数据集。测试数据库 `powerbi_mo_test` 包含：

- 一张 `sales` 表，共 7 列、5 行；
- 一张按类别汇总的 `sales_summary` 视图；
- 中文字符串、英文字符串、`DECIMAL(18,2)`、负数、`DATE`、
  `DATETIME(6)`、`BOOL` 和 `NULL`；
- 预期金额合计为 `1287.25`。

下一轮客户级验收应增加 TPC-H 数据集。TPC-H 能覆盖事实表/维表、多表 Join、
过滤、分组聚合、排序、Top N、日期范围和更有代表性的数据量；现有专项数据仍应
保留，用于覆盖中文、空值和 MatrixOne/ODBC 边界类型。

## 4. ODBC 测试结果

两个已注册驱动均通过冒烟测试：

- `MatrixOne ODBC 9.7 Unicode Driver`：通过；
- `MatrixOne ODBC 9.7 ANSI Driver`：通过。

两个驱动分别完成 18 个深度测试用例：

- 实际通过：13；
- 已关联 MatrixOne Issue 的预期失败：5；
- 未知失败：0。

覆盖内容包括连接能力、目录 API、结果描述符、20 种 SQL 类型、Unicode、
预处理参数、浮点和 Decimal 精度、Power BI DirectQuery 形态 SQL、事务、
事务隔离、流式/分块读取、SQLSTATE、并发连接、超时和取消。

### 已知预期失败

- [matrixone#26967](https://github.com/matrixorigin/matrixone/issues/26967)：
  utf8mb4 `VARCHAR` 线协议长度导致 ODBC `ColumnSize` 变小，
  `VARCHAR(128)` 被报告为 96；
- [matrixone#26769](https://github.com/matrixorigin/matrixone/issues/26769)：
  不支持仅有 `OFFSET`、没有 `LIMIT` 的语法；
- [matrixone#26716](https://github.com/matrixorigin/matrixone/issues/26716)：
  `VARBINARY` 返回了错误的线协议类型/标志；
- [matrixone#26715](https://github.com/matrixorigin/matrixone/issues/26715)：
  不接受未加反引号的 Unicode 标识符；
- [matrixone#26678](https://github.com/matrixorigin/matrixone/issues/26678)：
  查询超时参数可设置，但服务端没有执行超时中断。

完整控制台输出见 `odbc-test-release-candidate.log`。

## 5. Power BI Desktop 实测结果

Power BI Desktop 成功加载 `MatrixOne.mez`，并连接
`powerbi_mo_test`：

- Import：通过。Navigator 可以看到表和视图，5 行数据全部载入；中文、
  Decimal、日期、日期时间、布尔值和空值显示正确；
- DirectQuery：通过。存储模式明确显示为 DirectQuery；表格视觉对象按
  `category` 分组并汇总 `amount`，结果为 `A=30`、`B=30`、
  `Power BI=-7.25`、`上海=1234.50`，合计 `1287.25`；
- MatrixOne `system.statement_info` 确认 Power BI 向
  `powerbi_mo_test.sales` 发送了分组、排序和 `LIMIT 501` 查询。

### Power BI 覆盖矩阵

| 场景 | 状态 | 本轮证据或缺口 |
| --- | --- | --- |
| 自定义连接器发现和基本认证 | 已验证 | Desktop 成功加载连接器并连接 MatrixOne |
| Navigator 表/视图发现 | 已验证 | 能看到 `sales` 和 `sales_summary` |
| Import 基本数据类型 | 已验证 | 5 行专项数据完整导入 |
| DirectQuery 基本视觉对象 | 已验证 | 分组求和表格视觉对象结果正确 |
| DirectQuery SQL 下发 | 部分验证 | 观察到分组、排序和限制；尚未逐项验证全部折叠算子 |
| 筛选器、切片器和交叉筛选 | 未验证 | 尚未操作并核对生成 SQL |
| 多表关系和 Join 折叠 | 未验证 | 当前只有单表视觉对象 |
| Power Query 转换和查询折叠 | 未验证 | 尚未跑 Microsoft Power Query SDK 标准测试套件 |
| DAX 度量、Top N、DistinctCount、日期表 | 未验证 | 当前只做简单 `SUM` |
| 大数据量和性能 | 未验证 | 只有 5 行，未测并发视觉对象和响应时间 |
| Desktop 手动/重复刷新和数据变更可见性 | 未验证 | 仅验证首次加载和查询 |
| Power BI Service 发布 | 未验证 | 未发布到 Service |
| On-premises data gateway | 未验证 | 未验证计划刷新和 Gateway DirectQuery |
| TLS CA/主机名校验 | 未验证 | 本地默认非 TLS 部署 |
| 断网、重连、超时和取消的 Desktop 体验 | 未验证 | ODBC API 层有测试，Desktop UI 层没有 |

## 6. 安装包验证

- MSI 构建：通过；
- MSI 管理提取：通过；
- MSI/ZIP 包含 Unicode/ANSI 驱动、安装工具、`libmysql.dll`、
  OpenSSL/Kerberos/SASL 运行库、认证插件和 `MatrixOne.mez`；
- 仅把解压目录加入 DLL 搜索路径时，`myodbc9w.dll`、
  `myodbc9a.dll`、`myodbc9S.dll` 均可正常加载；
- 包内驱动和客户端 DLL 的 SHA-256 与通过测试的构建产物一致；
- 运行版 ZIP 不含 SDK 头文件、导入库、PDB 或测试工具。

## 7. 尚未完成的发布门槛

当前 MSI 和 Power BI 连接器均未签名，并且是预览产物。正式向客户承诺兼容前，
至少还应完成：

1. TPC-H 多表 Import/DirectQuery 测试，并记录每类视觉对象的生成 SQL；
2. Microsoft Power Query SDK DirectQuery 标准测试和查询折叠测试；
3. 切片器、交叉筛选、多表关系、常用 DAX、刷新和数据变更测试；
4. 10 万、100 万及更大规模下的导入速度、DirectQuery P95 延迟和并发测试；
5. Power BI Service 与标准模式 on-premises data gateway 测试；
6. 干净 Windows 虚拟机上的 MSI 安装、升级、修复和卸载；
7. ODBC DLL 代码签名、Power Query 连接器签名以及生产 TLS 测试。

## 8. 发布形式建议

Windows 客户预览阶段不需要额外发布到 GitHub Packages。GitHub Release 中的
MSI 是主要安装包，ZIP 用于免安装诊断，`.mez` 供连接器单独升级或调试。

正式交付时建议提供带签名的 MSI（内含驱动和匹配版本的 Power BI 连接器），
同时保留 ZIP、独立连接器、SHA-256、SBOM/第三方许可证和中文发布说明。若后续
支持 Linux，再分别发布 `.deb`/`.rpm`；GitHub Packages、NuGet 或 npm 并不是
Windows ODBC 驱动的必要分发渠道。
