# 暗黑破坏神 II：狱火重生 3D 相机

为《暗黑破坏神 II：狱火重生》（Diablo II: Resurrected，简称 D2R）添加相机控制功能。

按 F12 激活，保留游戏原有的投影方式、画面构图和相机角度。鼠标滚轮仅用于拉近或拉远视角：正交投影调整缩放比例，透视投影调整相机距离，不改变视场角（FOV）。按住鼠标中键拖动即可旋转相机。每次激活都会从原始视角开始。

独立版还支持原生 Tab 地图整体跟随相机旋转，包括神坛、传送点、宝箱和入口图标。地图保留原生比例，图标与地形一起旋转，不单独保持直立，也不启用贴合游戏地面的投影。

![](img/diablo.jpg)
![](img/bloodraven.jpg)
![](img/pindleskin.jpg)

## 独立版

**原项目提示：本程序会触发游戏的完整性检查，导致游戏在约一分钟后崩溃。**

### 下载与安装

可从原项目的 [GitHub 发布页面](https://github.com/emmericp/D2R-3D/releases) 或 [3d.diablo.deadlybossmods.com](https://3d.diablo.deadlybossmods.com/releases/d2r-3d-latest.zip) 下载。

以上链接指向原项目的预编译版本，不一定包含本仓库的修改。如需使用本仓库的修改，请从源码构建。

将 `.exe` 和 `.dll` 文件直接放入游戏目录，与 `D2R.exe` 位于同一文件夹中。通过 `d2r-3d.exe` 启动游戏，进入游戏后按 F12 激活。

### 从源码构建

安装 [Bazel](https://bazel.build/install/windows)，然后在仓库根目录执行：

```sh
bazel build //standalone:dist
```

构建产物位于 `bazel-bin/standalone/dist/d2r-3d.{exe,dll}`。

## 插件版

执行以下命令可构建两个插件，分别用于相机控制和渲染距离调整：

```sh
bazel build //plugin:dist
```

按 F12 同时启用两个插件，按 Shift+F12 仅启用相机插件。
