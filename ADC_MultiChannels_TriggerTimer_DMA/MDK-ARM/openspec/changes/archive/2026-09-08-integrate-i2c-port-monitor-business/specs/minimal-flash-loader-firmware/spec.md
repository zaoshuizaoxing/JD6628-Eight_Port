## MODIFIED Requirements

### Requirement: I²C 驱动与业务解耦
软件 I²C 模块 SHALL 保留初始化和寄存器读写能力，并 SHALL 提供使用软件端口和7位地址的通用定长命令读取能力；周期轮询、设备地址表、校验、端口映射、功率与温度处理 MUST 位于独立端口监控业务模块，MUST NOT 放入I²C驱动或 `main.c`。端口监控业务 SHALL 仅由LCD页面模式启动，Flash烧录模式 MUST 不执行I²C轮询。

#### Scenario: I²C底层访问
- **WHEN** 调用软件I²C初始化后对有效端口和7位从机地址执行命令读取
- **THEN** 驱动使用现有开漏GPIO完成起止、地址、字节收发、ACK/NACK和有限故障处理

#### Scenario: LCD模式启动业务
- **WHEN** LCD页面模式启用且Flash烧录模式关闭
- **THEN** LCD应用通过端口监控公共接口启动500 ms轮询，不直接包含I²C协议实现

#### Scenario: Flash烧录模式隔离
- **WHEN** Flash烧录模式启用且LCD页面模式关闭
- **THEN** 系统不调用端口监控轮询接口且不产生产品I²C查询事务
