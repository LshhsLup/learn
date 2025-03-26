#!/bin/bash

# 创建新分支 no-build 并切换到该分支
git checkout -b no-build

# 从工作区和暂存区移除 build 文件夹
git rm -r build

# 提交更改
git commit -m "Remove build folder"

# 推送新分支到远程仓库
git push -u origin no-build