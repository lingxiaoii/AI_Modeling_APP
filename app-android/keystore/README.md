# 固定调试签名密钥库（DEBUG-ONLY，非正式签名）
#
# 用途：CI 与本地统一使用此密钥库签名 debug APK，保证覆盖安装不要求先卸载。
# 密码：storepass=android / keypass=android / alias=androiddebugkey
#       （Android 官方 debug keystore 标准公开密码，非凭据；D-049 不适用）
# 禁止：任何正式签名/上架密钥入库（正式密钥仍是机密，永不进仓库）。
# 生成命令（与 Android SDK 默认 debug.keystore 等价）：
#   keytool -genkeypair -keystore debug.keystore -alias androiddebugkey \
#     -keyalg RSA -keysize 2048 -validity 10000 -storepass android -keypass android \
#     -dname 'CN=Android Debug,O=Android,C=US'
