# ADR-004: Theme And Authoring

状态：Accepted Draft

## Context

WhatsUI 需要一套易写、易读、易维护的 UI 编写方式，同时还需要稳定的样式分层和复合控件复用模型。

如果引入 XML/QML/HTML/CSS 风格描述层，或引入复杂的样式匹配系统，框架复杂度会明显上升。

## Decision

UI 编写方式采用强类型 C++ 组合 API：

- 页面对象负责局部状态与结构编排。
- builder 风格 API 负责表达树结构。
- 复合控件优先通过组合已有控件完成。

样式体系采用三层：

- `Theme`：全局 token
- widget style：`TextStyle`、`BoxStyle`、`ButtonStyle`、`InputStyle`
- control state：`normal`、`hover`、`pressed`、`focused`、`disabled`

样式解析顺序固定为：

1. 框架默认值
2. 主题展开后的默认控件样式
3. variant
4. 控件实例级显式覆盖
5. control state 修正

明确不做：

- CSS cascade
- selector engine
- 字符串样式表
- 浏览器式样式继承语义

## Consequences

- 页面写法保持 C++ 原生、类型明确、IDE 友好。
- 主题系统足够稳定，便于后续沉淀设计语言。
- 复合控件可以稳定复用 token 和控件样式，而不是复制一堆视觉常量。
- 样式层不会演化成另一个前端语言系统。
