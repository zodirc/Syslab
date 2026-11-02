# SysLab 使用手册

SysLab 的每一节从一类应用现象写起，再说明它依赖的那一层机制，并用程序按图中的顺序核对。用来区分问题落在语言规则、调用约定、内存、同步、调度，还是一次观测样本上。

设计全文在 `Syslab.md`。和实现不一致的决定在 `docs/decisions/`。

## 1. 环境

需要：

- Linux
- CMake 3.16 或更高
- Clang，并且能使用 `-std=c++20`
- Python 3，只用于 `make new` 和度量汇总

GCC 8 不接受 `-std=c++20`，不能构建本仓库。没有 Ninja 也可以，CMake 会使用 Makefiles。`CMakePresets.json` 的 version 是 2，CMake 3.19 及以上可以读取。本机 CMake 3.20.2 能列出 debug、release、relwithdebinfo、asan 和 tsan 预设。`make asan` 与 `make tsan` 把 Sanitizer 参数直接传给 CMake，输出目录分别是 `build/asan` 和 `build/tsan`。

可选工具：`objdump`、`readelf`、`strace`。工具路径由 `which` 解析，观测记录保存该路径和 `--version` 或 `-V` 的首行。没有 `perf` 时，性能计数器记为 unavailable，实验不会因此编造数字。清单里的 `performance_counter` 还要写 `perf_events`。`.clang-format` 使用 2 空格缩进、列宽 200。`make format-check` 对 `src` 和 `include` 执行 `clang-format --dry-run -Werror`。本机没有 `clang-format` 可执行文件，这项检查还没有跑过。

## 2. 上课

在仓库根目录：

```bash
make web
make learn N=1
make learn N=next
```

`make web` 在本机回环地址提供页面：[http://127.0.0.1:9527/](http://127.0.0.1:9527/)。节次按基础、进阶、方向排列，页首可以在这三层之间跳转，目录打开时展开基础。每节先显示背景，再显示标题。标题下面是重点，然后是上一事实、这一事实、下一事实，接着是图、名词和规则。下一事实带下一节的名词。换一组时仍写下一事实，并标出那一组的名字；换层时标成进入下一层。背景写明所在层次的处理过程及可观察结果。成立的事实已经写出结果时，机制和程序收在折叠区，避免把同一条因果再讲一遍。一组的第一节在名词之后给出这一组。有填数的课在原理里按「对照」看规则结果。原理之后是「先写下预期」，框里的提示就是本节要回答的问题。然后是成立的事实。本机演示按「对照观测」看程序输出，最后写明证明到哪一层。规则使用该课所依据的标准里的名称，例如 search permission、O_NOFOLLOW、packed 与 planar 采样、stride、pageLSN、idempotency key。本课没有实现的相邻机制会写在原理或边界里。浅拷贝、补码、fork 之后父进程的读数、互斥量保护下的计数和按页读写可以在页面上改输入，观察同一条规则；本机执行仍使用程序里的固定输入。浅拷贝、长度前缀和加锁顺序三节有按时间播放的图，帧里的数值与程序核对的数值相同。程序和核对收在折叠区，源码另收一处。成立的事实已经写在正文里时，本机演示不再重复同一段验证条件。清单里的 `prerequisites` 显示为「先看」，链接文字是那一节的标题。基础从浅拷贝开始，到有定义的共享计数结束。进阶先接其他锁和内存序，再扩到进程、分配、编译器能改写的部分、可执行文件、缓存和协程。方向按页与日志、连接与队列、命名空间、访问控制、采样和帧、步骤和工具分线。一条线结束时页面标成另一方向。本机演示里的「执行」在当前主机编译并运行该节，观测按程序打印的顺序出现。服务绑定 `127.0.0.1`。请求的 Host 必须是 `127.0.0.1:<端口>` 或 `localhost:<端口>`。表单里有服务启动时生成的口令，`POST /run/` 必须提交这个口令；请求若带 Origin，Origin 必须是 `http://127.0.0.1:<端口>` 或 `http://localhost:<端口>`。已有一节在执行时，再提交 `/run/` 会得到 409。客户端套接字的接收和发送超时是 5 秒。更换端口使用 `make web PORT=9528`。Ctrl+C 停止服务。

补码与 `float` `1.0` 的 binary32 位型在基础层。度量 `1065353216` 的内存字节为 `3f 80 00 00`，即 `0x3f800000`。

`make learn N=next` 从上次结束的下一节继续。通过，或这台机器明确做不了，都会记下进度。

优化会改写指令、但不改变语言结果的两节（被取地址的局部变量，以及抽象机结果固定为 6）会连续跑未优化和优化两次。两次结果相同，只说明抽象机结果还在。指令是否被改写，看「本机演示」里的反汇编摘录。

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
| `make test` | 运行单元测试、目录一致性检查和 `CPU-001`。不包含标签 `stress` 和 `experiments` |
| `make format-check` | 检查 `src` 与 `include` 是否符合 `.clang-format` |
| `make validate` | 检查实验目录、索引和课程标识 |
| `make asan` | 编译 AddressSanitizer 与 UndefinedBehaviorSanitizer，目录 `build/asan` |
| `make tsan` | 编译 ThreadSanitizer，目录 `build/tsan` |
| `make new ID=LAB-001 NAME=my-check MODULE=lab.custom DESC='一句话说明'` | 生成一个最小实验目录 |

也可以直接调用已编译的程序。它和 `libsyslab.a` 在同一目录：

```bash
./build/debug/syslab --help
./build/debug/syslab list --module atomic
./build/debug/syslab run CPU-004
./build/debug/syslab run CPU-001 --no-cache
./build/debug/syslab clean --keep 5
./build/debug/syslab inspect CPU-004 --artifact disassembly
./build/debug/syslab toolchain list
./build/debug/syslab validate --all
```

`--no-cache` 强制重新编译，并且不把该次编译写入 `artifacts/cache/`。`clean --keep N` 按实验保留最近 N 个运行目录，以及这些运行引用的构建目录。

`--format json` 把结构化结果写到标准输出，诊断写到标准错误。

### 退出码

| 码 | 含义 |
|---|---|
| 0 | 通过，或构建完成 |
| 2 | 实验或验证失败 |
| 3 | 本机不支持。`kernel_privileged` 和 `bare_metal` 在日常主机上得到这个码 |
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
| `*.perf.csv` | `perf stat -x,` 的原始输出。本机没有 `perf` 时不会生成 |

实验程序用下面的协议报告结果。`SYSLAB_STEP` 是证明过程中的一步观测，页面按打印顺序展示。

```text
SYSLAB_RESULT status=pass
SYSLAB_METRIC name=1
SYSLAB_NOTE basis=LANG text=说明里不要换行
SYSLAB_STEP 这一步观察到的事实
```

`status` 可以是 `pass`、`fail` 或 `unsupported`。`basis` 只能是 `LANG`、`ABI`、`COMPILER`、`OS`、`ISA`、`MICROARCH`、`OBSERVED`。重复的结果行、无法识别的状态或依据、缺少 `name=value` 的度量、空的步骤，以及未知的 `SYSLAB_` 前缀，都会使清单里的 `result_marker` 检查失败。

报告第 12 节在只有一次运行时写明样本数为 1，不输出离散程度。重复运行时，整数度量给出次数、最小值、中位数、最大值和四分位距。偶数个样本的中位数取排序后下标 `(n - 1) / 2`。四分位下标是 `(n - 1) / 4` 和 `3 * (n - 1) / 4`。非整数度量只列出原文。Debug 变体在这一节写明“调试构建，不作性能结论”。环境快照还记录 `cpufreq_governor`、`load_average` 和运行进程继承的 CPU 亲和性。读不到的字段带有状态和原因。

多次运行的度量也可以用：

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
- `make env` 里 `perf` 或 `perf_events` 为 unavailable：本机没有该命令，或 `perf stat -e cycles -- /bin/true` 没有成功。需要计数器的实验仍可根据自己的数值检查通过。计数器观测保持不完整，报告不写 0。
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
| `docs/roadmap/optimization-plan.md` | 已识别的工程问题、优化方案和实施顺序 |
| `artifacts` | 运行产物，默认不提交 |

## 8. 已经在一台主机上执行过的范围

注册表当前有 79 个实验。2026-11-02 在已有方向线上又单独运行六节，结果都是 PASSED：`SEC-003` `artifacts/runs/batch-20261102T035556-24ab0e58`，`SEC-004` `artifacts/runs/batch-20261102T035603-9716b852`，`AV-004` `artifacts/runs/batch-20261102T035610-e22dcfb3`，`AV-003` `artifacts/runs/batch-20261102T035617-9b0a7f58`，`AG-004` `artifacts/runs/batch-20261102T035624-96d5d712`，`AG-005` `artifacts/runs/batch-20261102T035631-3829f421`。这次没有重跑全部 94 个实验。同日早些时候新增的七节也是 PASSED：`SEC-001` `artifacts/runs/batch-20261102T035006-7486dec8`，`SEC-002` `artifacts/runs/batch-20261102T035013-16155ec0`，`AV-001` `artifacts/runs/batch-20261102T035019-6d534305`，`AV-002` `artifacts/runs/batch-20261102T035026-e7d74e1f`，`AG-001` `artifacts/runs/batch-20261102T035032-693fe5f0`，`AG-002` `artifacts/runs/batch-20261102T035038-677e48a8`，`AG-003` `artifacts/runs/batch-20261102T035045-ecc068a3`。方向课按页与日志、连接与队列、命名空间、访问控制、采样和帧、步骤和工具分线。此前的方向课还包括页寻址、校验和、索引、加锁顺序、预写日志、检查点、长度前缀、多帧、tid、读期限、有界队列、PID 与挂载命名空间、UTS、网络命名空间、chroot 和地址空间上限。定长记录写回和一行回声已从课程中移除。2026-11-02 的全表记录当时是 64 个实验，见本段末尾。2026-11-02，在提交 `415eaa3` 的工作区上，`make test` 通过。该目标包含单元测试、`syslab validate --all` 和 `CPU-001`。同日第二次 `syslab run CPU-001` 的 `build.json` 里 `cache_hit` 为 true，`library_build_type` 为 Debug，`library_digest` 与 `build/debug/libsyslab.a` 的 SHA-256 相同，值为 `4777eb60dd37ace5d5191fc39ae0e1073d11784ecf46341d989142cbdc57ea96`。`syslab run CPU-001 --no-cache` 的 `cache_hit` 为 false。`syslab run CPU-001 --variant optimized` 通过，链接 `build/release/libsyslab.a`，`library_build_type` 为 Release。`make asan` 生成 `build/asan/syslab_tests`。`ASAN_OPTIONS=detect_leaks=0` 时该程序打印 `makecontext/swapcontext` 支持不完整的提示，然后以状态 0 结束。默认泄漏检测仍报告 `LockFreeStack<int>::push` 和 `test_epoch` 的未回收对象，合计 516 字节、33 次分配，进程以状态 1 结束。`cmake --list-presets` 在 CMake 3.20.2 上列出五个预设。对本机 `syslab web` 的检查中，错误 Host、错误口令和错误 Origin 返回 403，未知节次返回 404，并发的第二次 `/run/` 返回 409，完成的执行返回 303。此前一次 `syslab run` 覆盖 62 个实验：61 个 PASSED，`OS-010` 为 UNSUPPORTED，记录里没有 `OBJ-001` 和 `OBJ-002`。2026-11-02 稍后，`build/debug/syslab_experiments` 跑完全部 64 个已注册实验：63 个 PASSED，`OS-010` 为 UNSUPPORTED。`CPU-002` 的反汇编观测记录 `tool_path` `/usr/bin/objdump` 和版本 `GNU objdump version 2.30-79.el8`。`OS-007` 在没有 `perf` 的情况下通过，性能计数器观测不完整。`build/tsan/syslab_tests` 的 `fiber_coroutine_and_steal` 在 `__tsan_func_entry` 发生 SEGV，状态 66。没有在 GCC 8 或 AArch64 上做同样的运行，也没有做 Debug 与 Release 的全表对照。同日先加入的 `STO-001` 记录在 `artifacts/runs/batch-20261102T025743-ce66782d`，`SRV-001` 记录在 `artifacts/runs/batch-20261102T025750-14c78917`，结果都是 PASSED；这两节随后从课程中移除。当时的 `CTR-001` 直接调用 `unshare(CLONE_NEWPID)`，记录在 `artifacts/runs/batch-20261102T025757-b2d643fb`，结果 UNSUPPORTED，进程状态 3。当前 `CTR-001` 先建立用户命名空间并映射为 0。随后对当前方向课单独运行，结果都是 PASSED：`STO-002` `artifacts/runs/batch-20261102T030714-c68c244f`，`STO-003` `artifacts/runs/batch-20261102T030721-973df849`，`STO-004` `artifacts/runs/batch-20261102T030729-4e8905cb`，`STO-005` `artifacts/runs/batch-20261102T030736-305990fe`，`SRV-002` `artifacts/runs/batch-20261102T030744-70e7c090`，`SRV-003` `artifacts/runs/batch-20261102T030753-b5969ae7`，`SRV-004` `artifacts/runs/batch-20261102T030801-dc920c10`，`CTR-001` `artifacts/runs/batch-20261102T030808-46d436b7`，`CTR-002` `artifacts/runs/batch-20261102T030815-cb1baf90`，`CTR-003` `artifacts/runs/batch-20261102T030823-e44f3fa4`，`CTR-004` `artifacts/runs/batch-20261102T030829-96acab04`。`tests/integration/expectations.txt` 只保留 `OS-010 UNSUPPORTED`。2026-11-02 继续单独运行六节，结果都是 PASSED：`STO-006` `artifacts/runs/batch-20261102T031506-5fee6526`，`STO-007` `artifacts/runs/batch-20261102T031513-76c2c5ff`，`SRV-005` `artifacts/runs/batch-20261102T031520-f8749b6c`，`SRV-006` `artifacts/runs/batch-20261102T031528-461abf7c`，`CTR-005` `artifacts/runs/batch-20261102T031535-0afa3d29`，`CTR-006` `artifacts/runs/batch-20261102T031543-06a6f56a`。这次没有重跑全部 81 个实验。`ctest` 的 `catalog` 和 `course_html` 通过。
