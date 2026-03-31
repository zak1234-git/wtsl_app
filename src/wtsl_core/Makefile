# wtcore模块独立Makefile
# 功能: 单独编译wtcore目录生成静态库和动态库

# 目标架构，默认x64
ARCH ?= arm
# 编译器和工具
CROSS_COMPILE ?= arm-mix510-linux-

CC      := $(CROSS_COMPILE)gcc
AR      := $(CROSS_COMPILE)ar rcs
RM      := rm -rf
MKDIR   := mkdir -p
FIND    := find

# 自动查找所有子目录并生成 -I 选项
INCLUDES := $(shell find $(SRC_DIR) -type d -exec echo -I{} \;)
# 编译选项
#CFLAGS  := -Wall -Wextra -Werror -O2 -fPIC \
#           -I../../inc \
#		   -I./api \
#           -I../../inc/cjson \
#		   -I$(INCLUDES)

ifdef DEBUG
CFLAGS += -DCONFIG_APP_DEBUG -g
endif

CFLAGS  := -Wall -Wextra -O2 -fPIC -Wno-unused-result \
            -I../../inc \
 		   -I./api \
            -I../../inc/cjson \
 		   -I$(INCLUDES)

# 目录定义
SRC_DIR      := .
BUILD_DIR    := ../../build/wtsl_core/$(ARCH)
OBJ_DIR      := $(BUILD_DIR)/obj
LIB_DIR      := $(BUILD_DIR)/lib

# 库搜索路径 - 明确列出所有可能的cjson库位置
LIB_SEARCH_PATHS := -L../../lib -L../../lib/$(ARCH)

LDFLAGS := $(LIB_SEARCH_PATHS)
# 链接库
LDLIBS  := -lcjson -lmicrohttpd -lsle_host_arm

# 库文件定义
STATIC_LIB   := $(LIB_DIR)/libwtsl_core.a
SHARED_LIB   := $(LIB_DIR)/libwtsl_core.so

# 查找所有源文件
SRC_FILES := $(shell $(FIND) $(SRC_DIR) -name "*.c")

# 生成对应的目标文件路径
OBJ_FILES := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC_FILES))

# 默认目标：构建静态库和动态库
all: prebuild check-libs $(STATIC_LIB) $(SHARED_LIB) sync_to_prj

# 预编译步骤
prebuild:
	@echo "执行预编译脚本..."
	@sh cfg/PreBuild.sh

# 检查必要的库文件是否存在
check-libs:
	@echo "检查依赖库文件..."
	@echo "正在搜索cjson库在: $(LIB_SEARCH_PATHS)"
	
	# 检查cjson库
	@if ! [ -f "../../lib/libcjson.so" ] && ! [ -f "../../lib/libcjson.a" ] && \
	  ! [ -f "../../lib/$(ARCH)/libcjson.so" ] && ! [ -f "../../lib/$(ARCH)/libcjson.a" ]; then \
		echo "错误: 未找到libcjson库文件"; \
		echo "请确认libcjson.so或libcjson.a存在于以下目录之一:"; \
		echo "  ../../lib"; \
		echo "  ../../lib/$(ARCH)"; \
		exit 1; \
	fi
	
	# 检查microhttpd库
	@if ! [ -f "../../lib/libmicrohttpd.so" ] && ! [ -f "../../lib/libmicrohttpd.a" ] && \
	  ! [ -f "../../lib/$(ARCH)/libmicrohttpd.so" ] && ! [ -f "../../lib/$(ARCH)/libmicrohttpd.a" ]; then \
		echo "错误: 未找到libmicrohttpd库文件"; \
		exit 1; \
	fi

# 创建输出目录
$(shell $(MKDIR) $(dir $(OBJ_FILES)) $(LIB_DIR))

# 构建静态库
$(STATIC_LIB): $(OBJ_FILES)
	@echo "创建静态库 $@..."
	@$(AR) $@ $(OBJ_FILES)
	@echo "静态库创建完成: $@"

# 构建动态库
$(SHARED_LIB): $(OBJ_FILES)
	@echo "使用的库路径: $(LDFLAGS)"
	@echo "创建动态库 $@..."
	@$(CC) -shared $(LDFLAGS) -o $@ $(OBJ_FILES) $(LDLIBS)
	@echo "动态库创建完成: $@"

sync_to_prj:
	@echo "同步wtsl_core库..."
	@cp $(SHARED_LIB) ../../lib/$(ARCH)
	@cp $(STATIC_LIB) ../../lib/$(ARCH)
	@cp $(SHARED_LIB) ../../bin/$(ARCH)

# 编译源文件为目标文件
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "编译: $<..."
	@$(MKDIR) $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

# 清理编译产物
clean:
	@echo "清理wtcore编译产物..."
	@$(RM) $(BUILD_DIR)

# 显示帮助信息
help:
	@echo "wtcore模块编译工具"
	@echo "使用方法: make [选项] [目标]"
	@echo "目标:"
	@echo "  all           - 构建静态库和动态库 (默认)"
	@echo "  prebuild      - 仅执行预编译脚本"
	@echo "  clean         - 清理编译产物"
	@echo "  help          - 显示此帮助信息"
	@echo "选项:"
	@echo "  ARCH=<arch>   - 指定架构 (arm, arm64, x64, 默认x64)"
	@echo "  CJSON_LIB_PATH=<path> - 手动指定cjson库路径"

# 伪目标
.PHONY: all prebuild clean help check-libs