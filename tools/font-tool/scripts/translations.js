const FONT_TOOL_TRANSLATIONS = {
    '请先选择要处理的字体文件！': 'Please select font files to process first!',
    '请输入要保留的字符！': 'Please enter characters to keep!',
    '字体处理引擎尚未就绪，请稍候再试': 'The font processing engine is not ready. Please try again shortly.',
    '处理中...': 'Processing...',
    '处理后的字体': 'Processed Fonts',
    '所有字体处理完成！成功处理': 'All fonts processed successfully:',
    '字体处理完成！成功处理': 'Font processing completed successfully:',
    '个文件': ' files',
    '字体处理失败，没有成功处理任何文件': 'Font processing failed. No files were processed.',
    '字体处理过程中发生错误，请重试': 'An error occurred while processing the fonts. Please try again.',
    '开始处理字体': 'Start Processing',
    '下载字体文件': 'Download Files',
    '下载所有字体文件': 'Download All Files',
    '下载完整文件夹 (ZIP)': 'Download Complete Folder (ZIP)',
    '下载': 'Download',
    '压缩了': 'reduced by',
    '删除此处理后的字体': 'Remove this processed font',
    '包含目录结构和所有非字体文件': 'Includes the directory structure and all non-font files',
    '个单独文件': ' standalone files',
    '个文件夹文件': ' folder files',
    '已删除字体: ': 'Removed font: ',
    '已耗时: ': 'Elapsed: ',
    '处理完成，总耗时: ': 'Completed in: ',
    '秒': 's',
    '分钟': 'm',
    '小时': 'h',
};

window.translateText = function(text) {
    const language = window.CatimeLocale?.getLanguage()
        || localStorage.getItem('catime-language')
        || 'en';
    return language === 'zh' ? text : (FONT_TOOL_TRANSLATIONS[text] || text);
};

function localizeFontToolPage() {
    if (!window.CatimeLocale?.isChinese()) return;

    document.title = 'Catime - 字体简化工具';
    document.querySelector('meta[name="description"]')?.setAttribute(
        'content',
        'Catime 字体简化工具：在浏览器中批量处理字体文件，仅保留指定字符。',
    );

    const translations = {
        'Drop Font Files Here': '拖拽字体文件到这里',
        'Supports .ttf, .otf, .woff, .woff2': '支持 .ttf、.otf、.woff、.woff2',
        'Drag folders to automatically scan for fonts': '拖拽文件夹后会自动扫描其中的字体',
        'Font Simplifier': '字体简化工具',
        'Drag & Drop Font Files or Folders Here': '将字体文件或文件夹拖到这里',
        'Or Paste via Ctrl+V': '也可以使用 Ctrl+V 粘贴',
        'Drag folders to automatically scan for fonts in all subdirectories': '支持拖拽文件夹，并自动扫描所有子目录中的字体',
        'Select Files': '选择文件',
        'Scan results will appear here': '扫描结果会显示在这里',
        'Clear All Files': '清除所有文件',
        'Characters to Keep': '需要保留的字符',
        'Numbers + :.': '数字 + :.',
        'Numbers 0-9': '数字 0-9',
        'English Letters': '英文字母',
        'Alphanumeric': '字母与数字',
        'Preparing Font Engine': '正在准备字体处理引擎',
        'Loading processing engine...': '正在加载处理引擎...',
        'Start Processing': '开始处理字体',
        'Font processing engine is initializing, please wait...': '字体处理引擎正在初始化，请稍候...',
        'Processed Fonts': '处理后的字体',
        'Download Files': '下载文件',
        'Clear All': '全部清除',
        'Generating ZIP...': '正在生成 ZIP...',
        'Preparing...': '准备中...',
        '100% Local Processing. All calculations are done in your browser. Data is not uploaded to any server.': '100% 本地处理。所有计算都在浏览器中完成，数据不会上传到任何服务器。',
    };

    const skip = new Set(['SCRIPT', 'STYLE', 'NOSCRIPT']);
    const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
    const nodes = [];
    while (walker.nextNode()) nodes.push(walker.currentNode);

    nodes.forEach(node => {
        if (skip.has(node.parentElement?.tagName)) return;
        const text = node.textContent.trim();
        if (!translations[text]) return;
        node.textContent = node.textContent.replace(text, translations[text]);
    });

    const charactersInput = document.getElementById('charactersInput');
    if (charactersInput) {
        charactersInput.placeholder = '请输入要保留的字符，例如：0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz';
    }

    const clearButton = document.querySelector('.download-controls .btn-danger');
    if (clearButton) clearButton.title = '清除全部处理结果并重新开始';
}

localizeFontToolPage();
