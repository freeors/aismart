SUB_PATH := external/third_party/libsrtp

LOCAL_SRC_FILES += \
	$(SUB_PATH)/crypto/cipher/aes_gcm_ossl.c \
	$(SUB_PATH)/crypto/cipher/aes_icm_ossl.c \
	$(SUB_PATH)/crypto/cipher/cipher.c \
	$(SUB_PATH)/crypto/cipher/null_cipher.c \
	$(SUB_PATH)/crypto/hash/auth.c \
	$(SUB_PATH)/crypto/hash/hmac_ossl.c \
	$(SUB_PATH)/crypto/hash/null_auth.c \
	$(SUB_PATH)/crypto/kernel/alloc.c \
	$(SUB_PATH)/crypto/kernel/crypto_kernel.c \
	$(SUB_PATH)/crypto/kernel/err.c \
	$(SUB_PATH)/crypto/kernel/key.c \
	$(SUB_PATH)/crypto/math/datatypes.c \
	$(SUB_PATH)/crypto/math/stat.c \
	$(SUB_PATH)/crypto/replay/rdb.c \
	$(SUB_PATH)/crypto/replay/rdbx.c \
	$(SUB_PATH)/srtp/ekt.c \
	$(SUB_PATH)/srtp/srtp.c