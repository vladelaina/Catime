# Catime 统一导航管理系统

## 概述

为了便于维护和更新，Catime 项目采用了统一的导航管理系统。所有页面的导航都通过 `scripts/navigation.js` 集中管理，实现了"一处修改，全站更新"的效果。

## 工作原理

### 核心文件
- **`scripts/navigation.js`** - 导航管理核心文件
- 所有 HTML 页面都引用此文件来动态生成导航

### 自动功能
1. **路径识别**：自动识别当前页面位置
2. **相对路径处理**：根据页面深度自动调整资源路径
3. **活动状态**：自动为当前页面添加 `active` 类
4. **按钮适配**：根据页面类型显示不同的操作按钮

## 页面支持

### 主站页面
- `index.html` - 首页
- `guide.html` - 指南
- `about.html` - 关于
- `support.html` - 支持项目

### 工具页面
- `tools/font-tool/index.html` - 字体简化工具
- 未来的工具页面...

## 如何添加新页面

### 1. 在 HTML 中添加导航容器
```html
<!-- 页头/导航 - 由 navigation.js 动态生成 -->
<header class="main-header"></header>
```

### 2. 引入导航脚本
```html
<!-- 在 JavaScript 部分添加 -->
<script src="scripts/navigation.js"></script>
<!-- 或对于工具页面 -->
<script src="../../scripts/navigation.js"></script>
```

### 3. 更新导航配置（如果需要）
在 `scripts/navigation.js` 中的 `getCurrentPage()` 方法中添加新页面的识别逻辑。

## 如何添加新工具

在 `scripts/navigation.js` 的 `generateNavigation()` 方法中，找到工具下拉菜单部分，添加新的工具链接：

```javascript
<ul class="dropdown-menu">
    <li><a href="${prefix}tools/font-tool/index.html"${this.currentPage === 'font-tool' ? ' class="active"' : ''}><i class="fas fa-font"></i> 字体简化工具</a></li>
    <!-- 添加新工具 -->
    <li><a href="${prefix}tools/new-tool/index.html"${this.currentPage === 'new-tool' ? ' class="active"' : ''}><i class="fas fa-tool-icon"></i> 新工具名称</a></li>
</ul>
```

## 如何修改导航内容

### 1. 修改主导航项
在 `generateNavigation()` 方法中修改导航链接：

```javascript
<li><a href="${prefix}new-page.html"${this.currentPage === 'new-page' ? ' class="active"' : ''}>新页面</a></li>
```

### 2. 修改操作按钮
在 `generateActionButtons()` 方法中修改按钮内容。

### 3. 添加特殊页面逻辑
如果某个页面需要特殊的导航样式，可以在相应方法中添加条件判断。

## 优势

✅ **集中管理** - 只需修改一个文件即可更新全站导航
✅ **自动适配** - 自动处理路径和活动状态
✅ **易于扩展** - 添加新页面和工具非常简单
✅ **一致性保证** - 确保所有页面导航完全一致
✅ **维护简单** - 减少重复代码，降低维护成本

## 注意事项

1. **加载顺序**：确保 `navigation.js` 在其他依赖导航的脚本之前加载
2. **路径正确性**：新页面的路径识别需要在 `getCurrentPage()` 中正确配置
3. **样式依赖**：确保页面已引入主项目的 CSS 样式文件

## 故障排除

### 导航不显示
- 检查 `navigation.js` 是否正确引入
- 检查控制台是否有 JavaScript 错误
- 确认页面有 `<header class="main-header"></header>` 容器

### 活动状态不正确
- 检查 `getCurrentPage()` 方法中的页面识别逻辑
- 确认页面文件名与识别逻辑匹配

### 路径错误
- 检查 `getPathPrefix()` 方法中的路径处理逻辑
- 确认相对路径计算正确
