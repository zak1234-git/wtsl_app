# 项目Makefile
# 支持架构: arm, arm64, x64
# 功能: 将wtsl_core目录内容编译为静态库和动态库，同时编译主程序

# 目标架构，默认arm,HI2921
ARCH ?= arm

# 根据架构添加特定库路径
ifeq ($(ARCH), arm)
    ARCH_LIB_PATH := -L./lib/arm -Wl,-rpath=./lib/arm,-rpath=/home/wt/app
	# 编译器和工具
	CROSS_COMPILE ?= arm-mix510-linux-
	LDLIBS  := -lpthread -lcjson -lmicrohttpd -lwtsl_core -lsle_host_arm
else ifeq ($(ARCH), arm64)
    ARCH_LIB_PATH := -L./lib/arm64 -Wl,-rpath=./lib/arm64,-rpath=/home/wt/app
	CROSS_COMPILE ?= aarch64-none-linux-gnu-
	LDLIBS  := -lpthread -lcjson -lmicrohttpd -lwtsl_core -lsle_host_arm64
else ifeq ($(ARCH), x64)
    ARCH_LIB_PATH := -L./lib/x64
else
    $(error 不支持的架构: $(ARCH), 支持的架构为arm, arm64, x64)
endif


CC      := $(CROSS_COMPILE)gcc
AR      := $(CROSS_COMPILE)ar rcs
RM      := rm -rf
MKDIR   := mkdir -p
FIND    := find

# 自动查找所有子目录并生成 -I 选项
INCLUDES := $(shell find $(SRC_DIR) -type d -exec echo -I{} \;)
# 编译选项
CFLAGS  := -Wall -Wextra -O2 -fPIC -Wno-unused-result \
		   -I$(INCLUDES)
ifdef DEBUG
CFLAGS += -DCONFIG_APP_DEBUG -g
endif

# 链接库 - 按依赖顺序排列，cJSON需要放在使用它的库之前
#LDLIBS  := -lpthread -lcjson -lmicrohttpd -lwtsl_core -lsle_host

# 目录定义
SRC_DIR      := src
WTCORE_DIR   := $(SRC_DIR)/wtsl_core

# 输出目录
OBJ_DIR      := obj/$(ARCH)
BIN_DIR      := bin/$(ARCH)
LIB_DIR      := lib/$(ARCH)
WTCORE_OBJ_DIR := $(OBJ_DIR)/wtsl_core

# 库文件定义
WTCORE_STATIC_LIB := $(LIB_DIR)/libwtsl_core.a
WTCORE_SHARED_LIB := $(LIB_DIR)/libwtsl_core.so

# 目标文件定义
TARGET       := $(BIN_DIR)/wtsl_app

# 基础库路径
BASE_LIB_PATHS := -L./lib

# 组合所有库路径
LDFLAGS := $(BASE_LIB_PATHS) $(ARCH_LIB_PATH) -L$(LIB_DIR)

# 查找wtcore源文件和其他源文件
WTCORE_SRC_FILES := $(shell $(FIND) $(WTCORE_DIR) -name "*.c")
OTHER_SRC_FILES  := $(shell $(FIND) $(SRC_DIR) -name "*.c" | grep -v "$(WTCORE_DIR)")

# 生成对应的目标文件路径
WTCORE_OBJ_FILES := $(patsubst $(WTCORE_DIR)/%.c, $(WTCORE_OBJ_DIR)/%.o, $(WTCORE_SRC_FILES))
OTHER_OBJ_FILES  := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(OTHER_SRC_FILES))

DIR_CHECK := $(shell ls -A $(WTCORE_DIR) | wc -l | awk '{print $$1}')
ifeq ($(DIR_CHECK),0)
# 默认目标：构建库和主程序
all: prebuild check-libs $(TARGET)
else
all: wtcore-libs $(TARGET)
endif
# 仅构建wtcore库
wtcore-libs: prebuild check-libs $(WTCORE_STATIC_LIB) $(WTCORE_SHARED_LIB) sync_to_prj

# 预编译步骤
prebuild:
	@echo "执行预编译脚本..."
	@sh cfg/PreBuild.sh
	@cd src/wtsl_core;cfg/PreBuild.sh;cd -

# 检查必要的库文件是否存在
check-libs:
	@echo "检查依赖库文件..."
	@if [ ! -f "lib/libcjson.so" ] && [ ! -f "lib/$(ARCH)/libcjson.so" ] && [ ! -f "lib/libcjson.a" ] && [ ! -f "lib/$(ARCH)/libcjson.a" ]; then \
		echo "错误: 未找到libcjson库文件"; \
		exit 1; \
	fi
	@if [ ! -f "lib/libmicrohttpd.so" ] && [ ! -f "lib/$(ARCH)/libmicrohttpd.so" ] && [ ! -f "lib/libmicrohttpd.a" ] && [ ! -f "lib/$(ARCH)/libmicrohttpd.a" ]; then \
		echo "错误: 未找到libmicrohttpd库文件"; \
		exit 1; \
	fi

# 创建输出目录
$(shell $(MKDIR) $(dir $(WTCORE_OBJ_FILES)) $(dir $(OTHER_OBJ_FILES)) $(BIN_DIR) $(LIB_DIR))

# 链接生成可执行文件
$(TARGET): $(OTHER_OBJ_FILES)
	@echo "链接 $@..."
	@echo "使用的库路径: $(LDFLAGS)"
	@$(CC) $(LDFLAGS) -o $@ $(OTHER_OBJ_FILES) $(LDLIBS)
	@echo "编译完成: $@"
	# 复制动态库到bin目录，方便运行
	@cp -f $(WTCORE_SHARED_LIB) $(BIN_DIR)/ 2>/dev/null || true
	@cp -f lib/libcjson.so $(BIN_DIR)/ 2>/dev/null || true
	@cp -f lib/$(ARCH)/libcjson.so $(BIN_DIR)/ 2>/dev/null || true
	@cp -f lib/libmicrohttpd.so $(BIN_DIR)/ 2>/dev/null || true
	@cp -f lib/$(ARCH)/libmicrohttpd.so $(BIN_DIR)/ 2>/dev/null || true

sync_to_prj:
	@echo "同步wtsl_core库..."
	@cp -f $(WTCORE_STATIC_LIB) $(LIB_DIR) || true
	@cp -f $(WTCORE_SHARED_LIB) $(LIB_DIR) || true
	
# 构建静态库
$(WTCORE_STATIC_LIB): $(WTCORE_OBJ_FILES)
	@echo "创建静态库 $@..."
	@$(AR) $@ $(WTCORE_OBJ_FILES)
	@echo "静态库创建完成: $@"

# 构建动态库 - 关键修改：添加cJSON库链接
$(WTCORE_SHARED_LIB): $(WTCORE_OBJ_FILES)
	@echo "创建动态库 $@..."
	@$(CC) -shared $(LDFLAGS) -o $@ $(WTCORE_OBJ_FILES) -lcjson -lmicrohttpd
	@echo "动态库创建完成: $@"
	
# 编译wtcore源文件为目标文件
$(WTCORE_OBJ_DIR)/%.o: $(WTCORE_DIR)/%.c
	@echo "编译wtcore: $<..."
	@$(MKDIR) $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

# 编译其他源文件为目标文件
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "编译: $<..."
	@$(MKDIR) $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

# 清理目标文件和可执行文件
clean:
	@echo "清理所有编译产物..."
	@$(RM) obj bin

# 清理特定架构的编译产物
clean-$(ARCH):
	@echo "清理 $(ARCH) 架构的编译产物..."
	@$(RM) obj/$(ARCH) bin/$(ARCH)

# 显示帮助信息
help:
	@echo "使用方法: make [选项] [目标]"
	@echo "目标:"
	@echo "  all           - 执行预编译，构建wtcore库并编译项目 (默认)"
	@echo "  wtcore-libs   - 仅构建wtcore静态库和动态库"
	@echo "  prebuild      - 仅执行预编译脚本"
	@echo "  clean         - 清理所有编译产物"
	@echo "  clean-<arch>  - 清理特定架构的编译产物"
	@echo "  help          - 显示此帮助信息"
	@echo "选项:"
	@echo "  ARCH=<arch>   - 指定架构 (arm, arm64, x64, 默认arm)"

# 伪目标
.PHONY: all prebuild clean clean-$(ARCH) help wtcore-libs check-libs