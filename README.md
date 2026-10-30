# SysLab 使用手册

SysLab 的每一节从一类应用现象写起，再说明它依赖的那一层机制，并用程序按图中的顺序核对。用来区分问题落在语言规则、调用约定、内存、同步、调度，还是一次观测样本上。

设计全文在 `Syslab.md`。和实现不一致的决定在 `docs/decisions/`。

## 1. 环境

需要：

- Linux
- CMake 3.16 或更高
- Clang，并且能使用 `-std=c++20`
- Python 3，只用于 `make new` 和度量汇总

GCC 8 不接受 `-std=c++20`，不能构建本仓库。没有 Ninja 也可以，CMake 会使用 Makefiles。

可选工具：`objdump`、`readelf`、`strace`。没有 `perf` 时，性能计数器记为 unavailable，实验不会因此编造数字。

## 2. 上课

在仓库根目录：

```bash
make web
make learn N=1
make learn N=next
```

`make web` 在本机回环地址提供页面：[http://127.0.0.1:9527/](http://127.0.0.1:9527/)。页首选择节次，标题旁为上一节与下一节。每节按这个顺序阅读：应用中的问题、这一层的机制、程序证明、证明到哪一层、源码。程序证明中的「执行」在当前主机编译并运行该节。程序按图中的顺序打印每一步观测，最后说明这些观测是否与图一致。监听地址仅接受本机连接。更换端口使用 `make web PORT=9528`。Ctrl+C 停止服务。

第 1 节从字节交换整数和浮点的现象写起，再核对补码与 `float` `1.0` 的 binary32 位型。度量 `1065353216` 的内存字节为 `3f 80 00 00`，即 `0x3f800000`。

`make learn N=next` 从上次结束的下一节继续。通过，或这台机器明确做不了，都会记下进度。

优化会改写指令、但不改变语言结果的两节（被取地址的局部变量，以及抽象机结果固定为 6）会连续跑未优化和优化两次。两次结果相同，只说明抽象机结果还在。指令是否被改写，看「程序证明」里的反汇编摘录。

按编号运行仍然可用，见下一节。编号对照在 `docs/experiments/index.md`。节次顺序在 `docs/learn/course.txt`。应用说明在 `docs/learn/brief.txt`。

## 3. 日常命令

下面的 `make` 目标都会在需要时先编译。

| 命令 | 作用 |
|---|---|
| `make web` | 在 http://127.0.0.1:9527/ 打开页面，用「执行」运行当前这一节 |
| `make learn` | 列出全部节，并写出页面 |
| `make learn N=1` | 在终端运行第 1 节 |
| `make learn N=next` | 接着下一节 |
| `make` | 只编译 |
| `make list` | 列出全部实验 |
| `make show ID=AT-003` | 看这个实验要弄清什么、检查什么、不能推出什么 |
| `make run ID=AT-003` | 运行，并在终端写出本次观测和检查范围 |
| `make run ID=AT-003 VARIANT=optimized` | 用 Release 变体 |
| `make run ID=AT-003 REPEAT=3` | 重复运行 3 次 |
| `make report ID=AT-003` | 打印最近一次报告 |
| `make env` | 查看本机工具是否可用 |
| `make test` | 运行单元测试和 `CPU-001` |
| `make new ID=LAB-001 NAME=my-check MODULE=lab.custom DESC='一句话说明'` | 生成一个最小实验目录 |

也可以直接调用已编译的程序。它和 `libsyslab.a` 在同一目录：

```bash
./build/debug/syslab --help
./build/debug/syslab list --module atomic
./build/debug/syslab run CPU-004
./build/debug/syslab inspect CPU-004 --artifact disassembly
./build/debug/syslab toolchain list
```

`--format json` 把结构化结果写到标准输出，诊断写到标准错误。

### 退出码

| 码 | 含义 |
|---|---|
| 0 | 通过，或构建完成 |
| 2 | 实验或验证失败 |
| 3 | 本机不支持。例如只有一个 NUMA 节点时的 `OS-010` |
| 4 | 运行被中断 |
| 1 | 用法错误或内部错误 |

`kernel_privileged` 和 `bare_metal` 在日常主机上直接得到退出码 3。没有开关可以跳过。

## 4. 一次运行里有什么

`make run ID=CPU-001` 之后，`artifacts/latest/CPU-001.txt` 里是本次运行目录。其中：

| 文件 | 内容 |
|---|---|
| `report.md` | 17 节报告 |
| `environment.json` | 主机事实。`status` 不是 `ok` 时，旁边的值不是测量结果 |
| `verification.json` | 每条检查的期望值和实际值 |
| `*.stdout` / `*.stderr` | 实验进程的输出 |
| `disassembly.txt`、`elf.json`、`*.strace` | 清单要求且工具可用时才会出现 |

实验程序用下面的协议报告结果。`SYSLAB_STEP` 是证明过程中的一步观测，页面按打印顺序展示。

```text
SYSLAB_RESULT status=pass
SYSLAB_METRIC name=1
SYSLAB_NOTE basis=LANG text=说明里不要换行
SYSLAB_STEP 这一步观察到的事实
```

`status` 可以是 `pass`、`fail` 或 `unsupported`。`basis` 只能是 `LANG`、`ABI`、`COMPILER`、`OS`、`ISA`、`MICROARCH`、`OBSERVED`。

多次运行的度量可以用：

```bash
python3 tools/data-analysis/summarize_metrics.py 某次运行目录/某文件.stdout
```

它输出每个度量的次数、最小值、中位数和最大值。单次样本的中位数就是该样本本身，不能写成硬件结论。

## 5. 加一个自己的实验

```bash
make new ID=LAB-001 NAME=my-check MODULE=lab.custom DESC='检查一个可以判定的结果'
```

这会创建 `experiments/custom/my-check/`，里面已有能通过校验的清单、一个返回通过的 `main.cpp`、`README.md` 和 `expected/behavior.md`。接下来只改 `main.cpp` 里的检查，并让 README 写明：

- 程序实际判定了什么
- 哪些现象只是观察，不能当作通过条件

然后：

```bash
make run ID=LAB-001
```

头文件在 `include/syslab/`。注释是中英对照：英文句子下面一行是中文。教学分配器、锁和协程都不是生产实现，头文件里写了对应限制。

公共输出辅助函数在 `experiments/common/check.hpp`：`syslab_pass`、`syslab_fail`、`syslab_unsupported`、`syslab_metric`、`syslab_note`。

## 6. 常见情况

- `make` 提示没有 `clang++`：安装 Clang。不要改用 GCC 8。
- `OS-010` 退出码 3：本机 NUMA 节点少于两个。这是预期结果，不是构建失败。
- `make env` 里 `perf` 为 unavailable：本机没有该命令。需要计数器的实验仍可根据自己的数值检查通过，计数器记录保持未采样。
- 报告或反汇编提示工具不可用：安装对应程序后重新 `make run`。旧报告不会被改写。
- 构建失败时，命令输出的目录里有 `build.log`。

## 7. 目录

| 路径 | 内容 |
|---|---|
| `Makefile`、`scripts/find_cxx.sh` | 降低第一次构建的步骤 |
| `include/syslab`、`src` | 机制库、运行器和命令行 |
| `experiments` | 实验清单、驱动和说明 |
| `docs/experiments/index.md` | 实验编号对照表 |
| `docs/architecture`、`docs/concepts` | 与代码对应的说明 |
| `artifacts` | 运行产物，默认不提交 |

## 8. 已经在一台主机上执行过的范围

开发主机上 `make test` 的单元测试和 `CPU-001` 已通过。`syslab run` 跑过 62 个随仓库提供的实验：61 个 PASSED，`OS-010` 为 UNSUPPORTED。没有在 GCC 8、AArch64、ASan 或 TSan 上做同样的全套运行。
