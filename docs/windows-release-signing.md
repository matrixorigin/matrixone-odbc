# Windows 发布签名与验收

正式 Windows 发布候选必须使用 `-RequireSigning`；默认无签名模式仅供 PR 开发测试。
不要把无签名 CI artifact 直接上传为客户 Release。

## 需要管理员提供什么

1. 公司身份的受公众信任的 **代码签名**证书（Code Signing EKU），不是 TLS 证书。
   私钥保存在硬件令牌/HSM/受控签名服务中，不提交源码、不发聊天、不导出到普通构建目录。
2. 当前实现通过 Windows `My` 证书库及 SignTool 调用私钥，支持已正确安装提供程序的硬件/KSP 身份。
   提供证书指纹、证书库位置（CurrentUser/LocalMachine）、CA 支持的 HTTPS RFC3161 时间戳地址。
   如采用仅 HTTP API 的云签名服务，需要另接该服务的签名适配器；此版本不宣称已支持。
3. 构建账户仅需该签名身份的使用权限，不需要导出私钥或租户/订阅 Owner 权限。
   硬件 PIN、服务登录和证书配置由管理员完成。不要在命令行或日志中暴露密码。
4. GitHub 管理员建立 `windows-release-signing` environment：仅 main、必需人工审批、禁止自审。
   配置专用 `matrixone-release-signing` runner（受限 runner group，仅此受保护工作流），不得承接 fork/PR 代码。
   GitHub 仓库登录不等于拥有代码签名身份。

Environment variables：`MYSQL_DIR`、`WIX_DIR`、`SIGNTOOL_PATH`、`CERT_THUMBPRINT`、
`CERT_STORE`、`TIMESTAMP_URL`。前三项指向管理员预装并校验过的工具/依赖。
环境变量中没有私钥密码。运行账户需要读取构建依赖、写入临时构建目录和调用签名提供程序。

## 签名顺序与强制检查

1. 校验工具路径、证书指纹、有效期、私钥可用性及 Code Signing EKU。
2. 构建并安装到 staging，递归签全部 DLL/EXE（包括安装工具、动态依赖和认证插件）。
   MatrixOne `myodbc*` 必须使用指定发布身份；第三方有效签名保留并验证时间戳。
   第三方未签名文件由发布身份签名时，应先由维护者确认再分发授权及来源。
   无效/损坏的既有签名直接报错，不覆盖掩盖问题。
3. 从已签名 staging 生成 ZIP 和 MSI；最后签 MSI。SHA256 文件摘要及 RFC3161 SHA256 时间戳。
4. SignTool `/pa /all /tw` 与 Authenticode 双重检查：不可信、签名失败、无时间戳均失败关闭。
5. 解压最终 ZIP、管理解包 MSI，逐个验签并与签名清单核对哈希，防止漏签或打包了旧文件。
6. 所有签名完成后才生成最终 SHA256 manifest。CI 只上传候选，不自动发布 GitHub Release。

本地示例（工具及证书由管理员预先配置）：

```powershell
./scripts/build-windows-packages.ps1 -PackageRevision 5 `
  -MySqlDir $env:MYSQL_DIR -WixDir $env:WIX_DIR `
  -BuildDirectory C:\build\mo5-signed -OutputDirectory C:\artifacts\mo5-signed `
  -RequireSigning -SignToolPath $env:SIGNTOOL_PATH `
  -CertificateThumbprint $env:CERT_THUMBPRINT -CertificateStore CurrentUser `
  -TimestampUrl $env:TIMESTAMP_URL
./scripts/Test-SignedWindowsPackage.ps1 -PackageDirectory C:\artifacts\mo5-signed `
  -SignToolPath $env:SIGNTOOL_PATH -ExpectedThumbprint $env:CERT_THUMBPRINT
```

## 客户验收不能被验签替代

- 在开启应用控制的干净 Windows 上检验全新安装、旧版升级、修复与卸载。
- 在 Power BI 中检验 DLL 实际加载、Import 刷新、MatrixOne 自定义连接器 DirectQuery、布尔切片器及分页。
- 检查 CodeIntegrity 日志无签名拦截；对照 MO 日志/查询证据确认请求确实到达服务端。
- `.mez` 是 Power Query 扩展，不作为 PE 文件交给 SignTool；其信任/分发流程独立于 ODBC DLL 签名。
- 本机验签成功不证明根证书来自公众信任 CA，也不保证企业白名单允许；管理员须确认身份链和目标策略。
- 不通过关闭 Smart App Control、添加广泛路径白名单或导入自签根证书规避验收。
- 本轮缺少签名身份，实际签名、时间戳服务、MSI 签名后解包和 Power BI 联调仍待执行。

参考：[Microsoft SignTool](https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool)。
