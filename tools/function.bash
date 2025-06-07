GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m'  

log_info() {
    echo -e "${BLUE}ℹ️  [INFO] $(date +'%Y-%m-%d %H:%M:%S')${NC} - $*"
}

log_success() {
    echo -e "${GREEN}✅ [SUCCESS] $(date +'%Y-%m-%d %H:%M:%S')${NC} - $*"
}

log_warning() {
    echo -e "${YELLOW}⚠️ [WARNING] $(date +'%Y-%m-%d %H:%M:%S')${NC} - $*"
}

log_error() {
    echo -e "${RED}❌ [ERROR] $(date +'%Y-%m-%d %H:%M:%S')${NC} - $*" >&2
    exit 1
}

set_error_handling() {
    set -euo pipefail
}

ensure_commands() {
    local missing_cmds=()
    for cmd in "$@"; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            missing_cmds+=("$cmd")
        fi
    done
    
    if [ ${#missing_cmds[@]} -gt 0 ]; then
        log_error "缺少以下命令：${missing_cmds[*]}，请先安装它们。"
    fi
}


install_apt_packages() {
    local packages_to_install=()
    
    sudo apt update

    if [ $# -eq 0 ]; then
        log_error "未提供任何要安装的包。"
        return 1
    fi

    for pkg_version in "$@"; do
        local pkg="${pkg_version%%=*}"
        local version_prefix="${pkg_version#*=}"

        # 获取已安装版本（如果有）
        local installed_version=""
        if installed_version=$(dpkg-query -W -f='${Version}' "$pkg" 2>/dev/null); then
            if [[ "$installed_version" == "$version_prefix"* ]]; then
                log_info "已安装 ${pkg} 版本 ${installed_version}"
            elif [ -n "$installed_version" ]; then
                log_info "已安装 ${pkg} 版本 ${installed_version}"
            else
                packages_to_install+=("$pkg")
            fi
        else
            log_info "包 ${pkg} 未安装，将添加到安装列表"
            packages_to_install+=("$pkg")
        fi
    done

    if [ ${#packages_to_install[@]} -eq 0 ]; then
        log_success "${description} 已全部安装，无需更新。"
        return 0
    fi

    log_info "安装 ${description}: ${packages_to_install[*]}"
    sudo apt install -y --no-install-recommends "${packages_to_install[@]}" || {
        log_error "安装 ${description} 失败。"
        return 1
    }

    log_success "${description} 安装完成。"
}


check_and_create_dir() {
    local dir_path="$1"
    local dir_name="${2:-目录}"
    
    if [ ! -d "$dir_path" ]; then
        mkdir -p "$dir_path"
        log_success "$dir_name 不存在，已创建：$dir_path"
    else
        log_info "$dir_name 已存在：$dir_path"
    fi
}


clean_dir() {
    local dir_path="$1"
    local dir_name="${2:-目录}"
    local use_sudo="${3:-false}"
    
    log_info "开始清理 $dir_name：$dir_path"
    if [ -d "$dir_path" ]; then
        if [ "$use_sudo" = "true" ]; then
            sudo rm -rf "$dir_path"
        else
            rm -rf "$dir_path"
        fi
        log_success "$dir_name 已清理：$dir_path"
    else
        log_warning "$dir_name 不存在，跳过清理：$dir_path"
    fi
}


check_ccache() {
    local no_ccache="${no_ccache:-false}"
    
    if command -v ccache >/dev/null 2>&1; then
        if [ "$no_ccache" = "false" ]; then
            log_success "已启用 ccache"
            export CC="ccache gcc"
            export CXX="ccache g++"
            # 设置 ccache 缓存大小
            ccache -M 10G >/dev/null 2>&1
        else
            log_warning "ccache 已禁用"
        fi
    else
        log_warning "未找到 ccache，将使用普通编译"
    fi
}


# 检查构建目录
check_build_dir() {
    local build_dir="${1:-build}"
    check_and_create_dir "$build_dir" "构建目录"
}

# 清理构建目录
clean_build_dirs() {
    local build_dir="${1:-build}"
    local use_sudo="${2:-false}"
    clean_dir "$build_dir" "构建目录" "$use_sudo"
}


check_root_warning() {
    if [[ "$EUID" -eq 0 ]]; then
        log_warning "脚本以 root 身份运行，建议使用普通用户并通过 sudo 安装。"
    fi
}


parse_cli_args() {
    local result=false
    while [[ "$#" -gt 0 ]]; do
        case $1 in
            --clean|-c)
                result=true
                shift
                ;;
            --help|-h)
                echo "Usage: ..."
                exit 0
                ;;
            *)
                log_error "未知参数: $1"
                shift
                ;;
        esac
    done
    echo "$result"
}

build_from_source() {
    local pkgname="$1" 
    local repo="$2" 
    local tag="$3" 
    local has_submodule="$4" 
    local cmake_opts="$5" 
    local method="$6"
    local version="${7:-tag}"
    

    local dir="/tmp/${pkgname}"

    if [[ -d "$dir" ]]; then
        log_info "目录已存在：${dir}"
    else
        log_info "克隆仓库：${repo} -> ${dir}"
        git clone --branch "$tag" --depth 1 "$repo" "$dir" || log_error "克隆${pkgname}失败。"
        if [[ "$has_submodule" == "true" ]]; then
            (cd "$dir" && git submodule update --init --recursive) || log_error "子模块更新失败：${pkgname}。"
        fi
    fi

    # 构建安装
    local build_dir="$dir/build"
    rm -rf "$build_dir" && mkdir -p "$build_dir"
    pushd "$build_dir" >/dev/null || { log_error "无法进入构建目录：${build_dir}"; return 1; }
    
    log_info "配置 CMake..."


    cmake -DCMAKE_BUILD_TYPE=Release ${cmake_opts}  .. || { log_error "CMake 配置失败：${pkgname}。"; popd >/dev/null; return 1; }
    
    log_info "编译源码..."
    make -j4 || { log_error "编译失败：${pkgname}。"; popd >/dev/null; return 1; }

    log_info "安装库..."
    case "$method" in
        "make_install")
            sudo make install || { log_error "make install 失败：${pkgname}。"; popd >/dev/null; return 1; }
            ;;
        "cpack_deb")
            cpack -G DEB || { log_error "生成 DEB 包失败：${pkgname}。"; popd >/dev/null; return 1; }
            sudo dpkg -i ./*.deb || { log_error "安装 DEB 包失败：${pkgname}。"; popd >/dev/null; return 1; }
            ;;
        "checkinstall")
            sudo checkinstall --pkgname="$pkgname" --pkgversion="$version" --default || { log_error "checkinstall 失败：${pkgname}。"; popd >/dev/null; return 1; }
            sudo dpkg -i ./*.deb || { log_error "安装 DEB 包失败：${pkgname}。"; popd >/dev/null; return 1; }
            ;;
        *)
            log_error "未知安装方法：${method}。请指定 make_install、cpack_deb 或 checkinstall。"
            popd >/dev/null
            return 1
            ;;
    esac

    popd >/dev/null

    # 删除临时目录
    rm -rf "$dir"

    log_success "${pkgname} 安装完成。"
    return 0
}

build_project() {
    local source_dir="$1"          # 源码根目录
    local clean_flag="$2"          # 是否清理构建目录: true/false
    shift 2                        # 移除前四个参数
    local cmake_args=("$@")        # 剩余的是 CMake 参数

    log_info "源码目录: $source_dir"
    log_info "清理构建目录: $clean_flag"


    # 切换到项目根目录
    cd "$source_dir" || log_error "无法切换到源码目录：$source_dir"

    # 清理构建目录（如启用）
    if [[ "$clean_flag" == true ]]; then
        clean_build_dirs "build" "true" || log_error "清理构建目录失败"
    else
        log_info "跳过清理，启用增量构建"
    fi

    # 检查构建目录
    check_build_dir "build"

    # 切换到构建目录
    cd "build" || log_error "无法进入构建目录"

    # 配置 CMake
    log_info "配置 CMake..."
    cmake "${cmake_args[@]}" .. || log_error "CMake 配置失败"

    # 编译
    log_info "开始编译..."
    make -j"$(nproc)" || log_error "编译失败"

    log_success "构建完成。"
}
