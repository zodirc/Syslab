# 仓库根目录的使用入口。
#   make learn        课程表
#   make learn N=1    上第 1 课
#   make learn N=next 接着上一课

BUILD ?= build/debug
JOBS  ?= $(shell nproc 2>/dev/null || echo 4)
SYSLAB := $(BUILD)/syslab

.PHONY: all demo test list run report show env help new learn web

all:
	@cxx=$$(sh scripts/find_cxx.sh); \
	echo "使用编译器 $$cxx"; \
	cmake -S . -B $(BUILD) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER="$$cxx"; \
	cmake --build $(BUILD) -j$(JOBS)

demo: learn

learn: all
	$(SYSLAB) learn $(N)

web: all
	$(SYSLAB) web $(PORT)

test: all
	ctest --test-dir $(BUILD) --output-on-failure

list: all
	$(SYSLAB) list

run: all
	@test -n "$(ID)" || { echo "用法: make run ID=CPU-001"; echo "可选: VARIANT=optimized REPEAT=3"; exit 1; }
	$(SYSLAB) run $(ID) $(if $(VARIANT),--variant $(VARIANT),) $(if $(REPEAT),--repeat $(REPEAT),)

report: all
	@test -n "$(ID)" || { echo "用法: make report ID=CPU-001"; exit 1; }
	$(SYSLAB) report $(ID) --latest

show: all
	@test -n "$(ID)" || { echo "用法: make show ID=CPU-001"; exit 1; }
	$(SYSLAB) show $(ID)

env: all
	$(SYSLAB) env --capabilities

new:
	@test -n "$(ID)" || { echo "用法: make new ID=LAB-001 NAME=my-check MODULE=lab.custom DESC='说明这句话'"; exit 1; }
	python3 scripts/new_experiment.py "$(ID)" "$(NAME)" "$(MODULE)" "$(DESC)"

help:
	@echo "make web             在 http://127.0.0.1:9527/ 打开页面"
	@echo "make learn           看课程表，并写出课程页"
	@echo "make learn N=1       上第 1 课"
	@echo "make learn N=next    接着上一课"
	@echo "make              编译 Debug 版本"
	@echo "make list         按编号列出实验"
	@echo "make run ID=AT-003"
	@echo "make report ID=AT-003"
	@echo "make show ID=AT-003"
	@echo "make env          查看本机工具能力"
	@echo "make test         运行 ctest"
	@echo "make new ID=LAB-001 NAME=my-check MODULE=lab.custom DESC='一句话说明'"
