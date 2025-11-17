# Arma3 Wiki 爬虫使用指南

## 项目介绍

**arma3-wiki-crawler** 是一个专门为 WTA 项目设计的 Wiki 爬虫，用于爬取 Bohemia Interactive 官方 Wiki 的所有内容，本地存储供大模型使用。

## 快速开始

### 1. 初始化 Submodule

```bash
# 如果还没有初始化 submodule
git submodule update --init --recursive

# 进入爬虫目录
cd arma3-wiki-crawler
```

### 2. 安装依赖

```bash
# 创建虚拟环境（推荐）
python -m venv venv
venv\Scripts\activate

# 安装依赖
pip install -r requirements.txt
```

### 3. 运行爬虫

```bash
# 爬取前 100 个页面（测试）
python -m crawler.main --max-pages 100

# 完整爬取所有页面
python -m crawler.main --full

# 爬取特定页面
python -m crawler.main --pages "SQF_Syntax,Scripting_Commands"

# 增量更新
python -m crawler.main --incremental
```

## 项目结构

```
arma3-wiki-crawler/
├── README.md                 # 详细文档
├── QUICKSTART.md            # 快速开始指南
├── requirements.txt         # Python 依赖
├── config.yaml              # 爬虫配置
├── crawler/
│   ├── __init__.py
│   ├── main.py              # 主爬虫程序
│   ├── fetcher.py           # 网页获取模块
│   ├── parser.py            # HTML 解析模块
│   ├── storage.py           # 本地存储模块
│   ├── indexer.py           # 搜索索引模块
│   └── utils.py             # 工具函数
├── data/
│   ├── wiki/                # 爬取的 Wiki 页面（Markdown）
│   ├── index.json           # 页面索引
│   ├── metadata.json        # 元数据
│   └── search_db.sqlite     # 全文搜索数据库
├── logs/
│   └── crawler.log          # 爬虫日志
└── tests/
    └── test_crawler.py      # 单元测试
```

## 核心功能

### 1. 网页爬取 (fetcher.py)

- 支持多线程并发爬取
- 自动重试机制（最多 3 次）
- 智能延迟控制，避免对服务器造成压力
- 完整的错误处理和日志记录

### 2. HTML 解析 (parser.py)

- 自动将 HTML 转换为 Markdown 格式
- 提取页面标题、目录、链接、代码块
- 支持表格、列表、代码块等元素转换
- 保留页面结构和格式

### 3. 本地存储 (storage.py)

- 将页面保存为 Markdown 文件
- 自动生成页面索引
- 记录爬取元数据（时间、大小等）
- 支持增量更新

### 4. 全文搜索 (indexer.py)

- 使用 SQLite FTS5 建立全文搜索索引
- 快速搜索相关文档
- 支持获取页面详细信息
- 统计索引信息

## 配置说明

编辑 `config.yaml` 自定义爬虫行为：

```yaml
# Wiki 源配置
wiki:
  base_url: "https://community.bistudio.com/wiki"
  start_page: "Main_Page"
  categories:
    - "Arma_3:_Scripting"
    - "Arma_3:_Mission_Editing"
  exclude_patterns:
    - "^File:"
    - "^Template:"

# 爬虫配置
crawler:
  max_workers: 4              # 并发线程数
  timeout: 30                 # 请求超时（秒）
  retry_times: 3              # 重试次数
  request_delay: 0.5          # 请求延迟（秒）
  max_pages: 0                # 最大页面数（0=无限制）

# 存储配置
storage:
  output_dir: "./data/wiki"
  index_file: "./data/index.json"
  metadata_file: "./data/metadata.json"
  search_db: "./data/search_db.sqlite"
```

## 数据输出

### 页面存储

爬取的页面保存为 Markdown 文件：

```
data/wiki/
├── SQF_Syntax.md
├── Scripting_Commands.md
├── Arma_3_Scripting_Commands.md
└── ...
```

### 索引文件 (index.json)

```json
{
  "pages": [
    {
      "title": "SQF Syntax",
      "url": "https://community.bistudio.com/wiki/SQF_Syntax",
      "filename": "SQF_Syntax.md",
      "last_updated": "2024-11-17T12:34:56Z",
      "size_bytes": 45678,
      "sections": ["Overview", "Variables", "Operators"]
    }
  ],
  "total_pages": 1234,
  "last_crawl": "2024-11-17T12:34:56Z",
  "version": "1.0"
}
```

### 搜索数据库 (search_db.sqlite)

SQLite 数据库，包含：
- `pages` 表：页面信息
- `sections` 表：页面章节
- `links` 表：页面链接
- `fts_pages` 虚拟表：全文搜索索引

## 大模型集成

### RAG 检索

```python
from crawler.indexer import WikiIndexer

indexer = WikiIndexer('./data')

# 搜索相关文档
results = indexer.search('SQF commands', top_k=5)

# 获取完整内容
for result in results:
    page_info = indexer.get_page_info(result['id'])
    print(f"Title: {page_info['title']}")
    print(f"Content: {page_info['content'][:500]}...")
```

### 微调数据集

```python
from pathlib import Path
import json

# 收集所有文档
documents = []
for md_file in Path('./data/wiki').glob('*.md'):
    with open(md_file, 'r', encoding='utf-8') as f:
        documents.append({
            'title': md_file.stem,
            'content': f.read()
        })

# 保存为 JSONL 格式
with open('training_data.jsonl', 'w') as f:
    for doc in documents:
        f.write(json.dumps(doc) + '\n')
```

## 性能指标

| 指标 | 值 |
|------|-----|
| 爬取速度 | ~100-200 页/分钟（4线程） |
| 存储大小 | ~500MB-1GB（全部Wiki） |
| 完整爬取时间 | ~30-60 分钟 |
| 搜索响应时间 | <100ms |

## 常见问题

### Q: 如何加快爬取速度？

A: 增加 `config.yaml` 中的 `max_workers` 和减少 `request_delay`：

```yaml
crawler:
  max_workers: 8
  request_delay: 0.2
```

### Q: 爬取中断了怎么办？

A: 使用增量更新继续：

```bash
python -m crawler.main --incremental
```

### Q: 如何清除已爬取的数据？

A: 删除 `data/` 目录（除了 `.gitkeep`）

### Q: 支持代理吗？

A: 支持。编辑 `config.yaml`：

```yaml
advanced:
  use_proxy: true
  proxies:
    - "http://proxy1.com:8080"
```

## 注意事项

⚠️ **遵守 robots.txt**：爬虫自动遵守 Wiki 的 robots.txt  
⚠️ **合理延迟**：默认 0.5 秒延迟，避免对服务器造成压力  
⚠️ **首次爬取**：可能需要 1-2 小时  
⚠️ **定期更新**：建议每周或每月更新一次  

## 许可证

MIT License

## 相关链接

- [Bohemia Interactive Wiki](https://community.bistudio.com/wiki)
- [Arma 3 Scripting](https://community.bistudio.com/wiki/Category:Arma_3:_Scripting)
- [SQF Syntax](https://community.bistudio.com/wiki/SQF_Syntax)
