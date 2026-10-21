# mculib - 一个统一的mcu驱动兼容层

## 一、目录结构

````
app/         应用层，特定设备的驱动都在这里
subsystems/  子系统层，API 契约
common/      提供一些通用工具函数的接口
adapter/     适配层，针对每一系列的mcu需要单独定制
config/      适配层，后端选择，针对每一个工程选择使用的硬件/外设定制
tools/       构建辅助脚本
```
