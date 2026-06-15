# ESP-IDF 环境配置 — 双环境（办公室 / 家里）
# ============================================
# Claude Code 会每次问你在哪里，然后选择对应的配置。
# 如果手动运行脚本，默认使用 $env_office 的配置。

# ----- 办公室环境 -----
$script:env_office = @{
    IDF_PATH       = 'D:\esp\v5.5.4\esp-idf'
    IDF_TOOLS_PATH = 'C:\Espressif\tools'
    COM_PORT       = 'COM3'
}

# ----- 家里环境 -----
$script:env_home = @{
    IDF_PATH       = 'D:\Espressif\frameworks\esp-idf-v5.5.4'
    IDF_TOOLS_PATH = 'D:\Espressif\tools'
    COM_PORT       = 'COM7'
}

# ----- 当前使用的环境（办公室 / 家里）-----
$script:selected = $script:env_office    # 🏢 办公室

$script:IDF_PATH       = $script:selected.IDF_PATH
$script:IDF_TOOLS_PATH = $script:selected.IDF_TOOLS_PATH
$script:COM_PORT       = $script:selected.COM_PORT
