# ✨ Welcome to **Catime**  
Your one-stop hub for our website, blog, book, and more!

✨ 欢迎来到 **Catime**
这里是我们的网站、博客、书籍等内容的集中入口！

---

## Local Development

```bash
pnpm install
pnpm dev
```

The Vite development server provides clean routes such as `/guide`, `/about`,
`/support`, and `/tray` without exposing `.html` filenames.

## Source organization

- `styles/style.css` is the stable global entry. It imports the ordered modules
  from `styles/modules/`, so both direct static hosting and Vite builds preserve
  the original cascade.
- Page-specific CSS is grouped under `styles/about/`, `styles/support/`,
  `tools/font-tool/styles/`, and `tray/styles/modules/`.
- Larger classic scripts are split by feature under `scripts/support/` and
  `tools/font-tool/scripts/`; their order in the corresponding HTML is part of
  the public page contract.

Run the complete production and browser smoke verification with:

```bash
npm run verify
```

---

## 📥 Quick Start
To clone only the `gh-pages` branch, run:

要仅克隆 `gh-pages` 分支，请执行以下命令：

```bash
git clone --branch gh-pages --single-branch https://github.com/vladelaina/Catime.git
```
