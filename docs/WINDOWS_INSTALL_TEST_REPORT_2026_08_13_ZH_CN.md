# MatrixOne ODBC Windows 安装与独立交付测试报告

测试日期：2026-08-13

## 本轮结论

MatrixOne ODBC 可以继续使用 MySQL 9.7 SDK 作为**构建输入**，但客户交付物
不需要安装或下载 MySQL SDK。本轮从全新构建目录生成
`9.7.0-mo.2` MSI 和便携 ZIP，ZIP 中头文件、C++ 头文件和 import library
数量均为 0；ANSI、Unicode 和 Setup DLL 可从包内依赖目录独立加载。

本轮发现并修复了三个会影响正式交付的问题：

1. `mo.1`、`mo.2` 原先共享 MSI `ProductVersion=9.7.0` 和 ProductCode，不能
   形成可靠升级链；现在下游修订映射为 MSI `9.7.N`，每个 `mo.N` 有稳定且
   不同的 ProductCode，ODBC ABI 仍保持 `9.7.0`。
2. WiX 构建错误依赖源码目录残留的生成文件，干净 checkout 会失败；版本、
   插件和依赖清单现全部在 binary tree 生成。
3. 客户机运行时依赖检查原先使用默认 DLL 搜索路径，不能正确模拟 ODBC
   Driver Manager；现使用安全的 DLL 所在目录搜索并将加载失败设为硬失败。

当前本机进程不是管理员，因此完整 MSI 写 HKLM 生命周期放到
`windows-2025` 干净管理员 runner 执行；相关 CI 会保存 MSI 日志和 JSON
证据。当前结论是：安装包可进入 PR/CI 验证，CI 全绿前不应发布稳定版。

## 基线与产物

- ODBC 源码：`main` 基线 `0ccc2c720d5ed35b0717d95569438ab396fdb024`；
- 构建 SDK：MySQL Server 9.7.1 Windows ZIP，仅构建机使用；
- WiX：5.0.2 portable administrative image；
- 当前 MSI：`ProductVersion=9.7.2`，ProductCode
  `{032AA2B6-7062-55C6-A8FF-17EF6156C493}`；
- UpgradeCode：`{0741D0F8-D27E-48E9-8D6C-70AC00A6E3AD}`；
- MSI SHA-256：
  `8a229b538aa40b7b821507c2cae13447fac4bf30c15594af7fa7945bbe59c6e8`；
- ZIP SHA-256：
  `0e0aa18fc5b8ff1cc91098dfe14d12c49600f374edba96c32a5e00ea2745155e`。

哈希只对应本轮本机构建证据；正式 Release 应使用 CI 产物重新生成并签名。

## 公开规范/测试的采用方式

| 公开来源 | 本轮映射 |
| --- | --- |
| Microsoft ODBC 安装 API 与 Windows ODBC 管理器 | 驱动/Setup 注册、绝对路径、系统 DSN、卸载清理 |
| Windows Installer major-upgrade 语义 | 不同 ProductCode、相同 UpgradeCode、递增 ProductVersion、阻止降级 |
| Oracle Connector/ODBC Windows 安装方式 | Oracle MySQL ODBC 与 MatrixOne ODBC 并存及相互卸载隔离 |
| pyodbc 官方测试 | 安装后的公共 ODBC API 和连接行为回归 |
| MySQL Connector/ODBC 上游 TAP 程序 | ABI、catalog、类型、诊断和异常路径差异测试 |
| Microsoft Power Query SDK ODBC 套件 | Import/DirectQuery、折叠与 Power BI connector 验证 |

公开套件没有覆盖 MatrixOne 下游包版本、真实 `mo.1` 升级和中文路径，所以下面
补充 10 个大场景。升级基线直接下载 GitHub Release 中真实的 `mo.1` MSI，
并校验公开 SHA-256，不用当前源码伪造旧包。

## 10 个安装与异常路径大场景

| # | 大场景 | 判定标准 | 状态 |
| ---: | --- | --- | --- |
| 1 | 干净机静默安装 + 带空格自定义目录 | 两个 x64 驱动注册；Driver/Setup 路径存在；不依赖已安装 MySQL | CI 自动化 |
| 2 | 便携 ZIP + 中文/空格/随机路径 | 无 SDK 文件；注册、DLL 加载、可选 MO 连接成功 | DLL/依赖本机通过；注册在 CI |
| 3 | 重复静默安装和 `/fa` repair | 修复成功且现有系统 DSN 不丢失 | CI 自动化 |
| 4 | 真实 `mo.1` → `mo.2` major upgrade | 旧包被替换、ProductCode 不同、DSN 保留 | CI 自动化 |
| 5 | `mo.2` → `mo.1` 降级 | 安装被阻止，当前驱动保持可用 | CI 自动化 |
| 6 | Oracle MySQL ODBC 同机共存 | 两品牌驱动同时存在；卸载 Oracle 后 MatrixOne 仍能加载 | CI 自动化 |
| 7 | 缺少包内 `libmysql.dll` | 加载以 Win32 126 失败；恢复文件后同进程重新加载成功 | 本机通过 |
| 8 | ANSI/Unicode/Setup 三 DLL 依赖闭包 | 使用 DLL 所在目录安全搜索，三者全部可加载并释放 | 本机通过 |
| 9 | 完整卸载和重装 | 驱动注册、文件、自定义目录清除；随后可重新安装 | CI 自动化 |
| 10 | 安装产物直连 MatrixOne | ANSI/Unicode smoke 均通过；deep 各 29 PASS/8 MO XFAIL/0 FAIL | 本机通过 |

## 安装产物直连 MatrixOne 结果

当前 `mo.2` 干净构建的测试可执行程序，通过已注册的同源码驱动直连最新
MatrixOne `main` 提交
`1d7b6311ce91df0968da435bb405f3d68137c595`：

- Unicode smoke：退出码 0；
- Unicode deep：29 PASS / 8 个已登记 MO XFAIL / 0 FAIL；
- ANSI smoke：退出码 0；
- ANSI deep：29 PASS / 8 个已登记 MO XFAIL / 0 FAIL。

8 个 MO XFAIL 仍是 #26967、#26769、#26715、#27034、#27035、#27024、
#27036 和 #26678；本轮安装代码没有产生新的协议回归。

最新 MO main 的公开套件差分如下：

- pyodbc：31 PASS / 7 个 wheel 私有调试接口缺失，7 项均在调用 ODBC 前失败；
- Connector/ODBC 35 个上游程序：370 TAP PASS / 116 FAIL，较旧 main 的
  332/131 改善，逐程序没有 PASS→FAIL 回退；
- Microsoft Power Query SDK：功能比较 206/212；
- Power Query 折叠诊断：75/212 命令文本匹配；官方严格口径为 Sanity 8/9、
  Standard 21/203。严格失败多数源于连接器为规避 MO #27034 暂停参数绑定，
  不能与功能正确率混算；
- Microsoft NYC Taxi 派生公开数据：10,000 + 10,000 + 265 + 1 行，四表
  SHA-256、装载和行数校验全部通过。

## 发布建议

ODBC 原生驱动应发布 Windows x64 MSI 和便携 ZIP，而不是把 NuGet/package
作为主要客户安装方式。Release 至少还应包含独立 `MatrixOne.mez`、
SHA-256、中文测试报告和已知问题。建议将 `mo.2` 标记为 prerelease：只有
Windows 安装 CI、最新 MO main 的公共 ODBC/Power Query 回归、恶意 TLS 和
Gateway 测试均通过后，再评估稳定版本与代码签名。
