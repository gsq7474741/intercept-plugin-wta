#这是一个监视器，可以监视 `%localappdata%\Arma 3` 目录下最新的后缀为rpt的文件，并在终端中输出，实现类似于watch + tail的功能

import os
import time
from pathlib import Path
from datetime import datetime

def get_latest_rpt_file(arma3_dir):
    """获取Arma 3目录下最新的.rpt文件"""
    rpt_files = list(Path(arma3_dir).glob("*.rpt"))
    
    if not rpt_files:
        return None
    
    # 按修改时间排序，返回最新的文件
    latest_file = max(rpt_files, key=lambda f: f.stat().st_mtime)
    return latest_file

def monitor_rpt_file():
    """监视最新的RPT文件并实时输出新内容"""
    # 获取Arma 3本地目录
    localappdata = os.environ.get('LOCALAPPDATA')
    if not localappdata:
        print("错误: 无法获取 LOCALAPPDATA 环境变量")
        return
    
    arma3_dir = Path(localappdata) / "Arma 3"
    
    if not arma3_dir.exists():
        print(f"错误: Arma 3 目录不存在: {arma3_dir}")
        return
    
    print(f"监视目录: {arma3_dir}")
    print("正在查找最新的 .rpt 文件...")
    
    current_file = None
    last_position = 0
    
    try:
        while True:
            # 检查是否有更新的rpt文件
            latest_file = get_latest_rpt_file(arma3_dir)
            
            if latest_file is None:
                print("未找到 .rpt 文件，等待中...")
                time.sleep(2)
                continue
            
            # 如果检测到新文件，切换监视目标
            if current_file != latest_file:
                current_file = latest_file
                last_position = 0
                print(f"\n{'='*60}")
                print(f"开始监视文件: {current_file.name}")
                print(f"文件修改时间: {datetime.fromtimestamp(current_file.stat().st_mtime)}")
                print(f"{'='*60}\n")
            
            # 读取文件新内容
            try:
                with open(current_file, 'r', encoding='utf-8', errors='ignore') as f:
                    # 移动到上次读取的位置
                    f.seek(last_position)
                    
                    # 读取新内容
                    new_content = f.read()
                    
                    if new_content:
                        print(new_content, end='')
                        last_position = f.tell()
                    
            except FileNotFoundError:
                print(f"警告: 文件 {current_file} 不存在")
                current_file = None
                continue
            except Exception as e:
                print(f"读取文件时出错: {e}")
            
            # 短暂休眠以降低CPU使用率
            time.sleep(0.5)
            
    except KeyboardInterrupt:
        print(f"\n\n{'='*60}")
        print("监视已停止")
        print(f"{'='*60}")

if __name__ == "__main__":
    print("=" * 60)
    print("Arma 3 RPT 文件监视器")
    print("按 Ctrl+C 停止监视")
    print("=" * 60)
    monitor_rpt_file()