#include <stddef.h>
#include <string.h>
#include <pspcrypt.h>
#include <pspkernel.h>
#include <pspmoduleinfo.h>
#include <pspthreadman_kernel.h>
#include <pspsdk.h>
#include <psptypes.h>
#include <psputils.h>

PSP_MODULE_INFO("TM_Type6Decrypt", 0x1000, 1, 0);

#define TYPE6_HEADER_SIZE 0x150
#define KIRK_HEADER_SIZE 0x90
#define KIRK_HEADER_OFFSET 0x40
#define SCRAMBLE_HEADER_SIZE 0x14

typedef struct {
	u32 tag;
	const u8 *key;
} Type6Key;

typedef struct {
	u8 tag[4];
	u8 empty[0x38];
	u8 signature_tail[0x20];
	u8 id[0x10];
	u8 sha1[0x14];
	u8 kirk_header[0x40];
	u8 kirk_metadata[0x10];
	u8 prx_header[0x80];
} Type6Header;

static const u8 keys636_k1[0x10] = {
	0x07, 0xE3, 0x08, 0x64, 0x7F, 0x60, 0xA3, 0x36,
	0x6A, 0x76, 0x21, 0x44, 0xC9, 0xD7, 0x06, 0x83
};

static const u8 keys636_k2[0x10] = {
	0x91, 0xF2, 0x02, 0x9E, 0x63, 0x32, 0x30, 0xA9,
	0x1D, 0xDA, 0x0B, 0xA8, 0xB7, 0x41, 0xA3, 0xCC
};

static const u8 keys639_k3[0x10] = {
	0x01, 0x7B, 0xF0, 0xE9, 0xBE, 0x9A, 0xDD, 0x54,
	0x37, 0xEA, 0x0E, 0xC4, 0xD6, 0x4D, 0x8E, 0x9E
};

static const u8 keys638_k4[0x10] = {
	0x98, 0x43, 0xFF, 0x85, 0x68, 0xB2, 0xDB, 0x3B,
	0xD4, 0x22, 0xD0, 0x4F, 0xAB, 0x5F, 0x0A, 0x31
};

static const Type6Key type6_keys[] = {
	{ 0x4C948AF0, keys636_k1 },
	{ 0x4C948BF0, keys636_k2 },
	{ 0x4C948CF0, keys639_k3 },
	{ 0x4C948DF0, keys638_k4 }
};

static const u8 *find_key(u32 tag)
{
	unsigned int i;

	for (i = 0; i < sizeof(type6_keys) / sizeof(type6_keys[0]); ++i) {
		if (type6_keys[i].tag == tag)
			return type6_keys[i].key;
	}

	return NULL;
}

static int scramble(u8 *buffer, u32 size, u32 code)
{
	u32 *command = (u32 *)buffer;

	command[0] = 5;
	command[1] = 0;
	command[2] = 0;
	command[3] = code;
	command[4] = size;

	return sceUtilsBufferCopyWithRange(buffer, size + SCRAMBLE_HEADER_SIZE,
		buffer, size + SCRAMBLE_HEADER_SIZE, 7);
}

static int scramble_data(u8 *data, u32 size, u32 code)
{
	u8 buffer[SCRAMBLE_HEADER_SIZE + 0x90] __attribute__((aligned(0x40)));
	int result;

	memset(buffer, 0, sizeof(buffer));
	memcpy(buffer + SCRAMBLE_HEADER_SIZE, data, size);
	result = scramble(buffer, size, code);
	if (result != 0)
		return result;

	/* KIRK CMD7 writes the decrypted payload at the start of the buffer. */
	memcpy(data, buffer, size);
	return 0;
}

static int expand_seed(u8 expanded[0x90], const u8 key[0x10], u32 code)
{
	u8 buffer[SCRAMBLE_HEADER_SIZE + 0x90] __attribute__((aligned(0x40)));
	unsigned int i;
	int result;

	memset(buffer, 0, sizeof(buffer));
	for (i = 0; i < 0x90; i += 0x10) {
		memcpy(buffer + SCRAMBLE_HEADER_SIZE + i, key, 0x10);
		buffer[SCRAMBLE_HEADER_SIZE + i] = i / 0x10;
	}

	result = scramble(buffer, 0x90, code);
	if (result != 0)
		return result;

	memcpy(expanded, buffer, 0x90);
	return 0;
}

static int decrypt_kirk_header(u8 output[0x40], const u8 input[0x40],
	const u8 xor_buffer[0x80], u32 code)
{
	unsigned int i;

	for (i = 0; i < 0x40; ++i)
		output[i] = input[i] ^ xor_buffer[i];

	if (scramble_data(output, 0x40, code) != 0)
		return -1;

	for (i = 0; i < 0x40; ++i)
		output[i] ^= xor_buffer[i + 0x40];

	return 0;
}

static int decrypt_type6(const u8 *input, u8 *output, u32 size, u32 capacity)
{
	u8 expanded_seed[0x90];
	u8 hash_input[0x14C];
	u8 digest[0x14];
	u8 *kirk_header;
	u32 tag;
	u32 decrypt_size;
	u32 hash_size = 0;
	const u8 *key;
	Type6Header header;
	int result;

	if (!input || !output || size < TYPE6_HEADER_SIZE || size > capacity ||
	    ((u32)output & 0x3F) != 0)
		return -2;

	memcpy(&tag, input + 0xD0, sizeof(tag));
	key = find_key(tag);
	if (!key)
		return -1;

	memcpy(&decrypt_size, input + 0xB0, sizeof(decrypt_size));
	if (decrypt_size == 0 || decrypt_size > size || decrypt_size > capacity)
		return -2;

	memset(&header, 0, sizeof(header));
	memcpy(header.tag, input + 0xD0, sizeof(header.tag));
	memcpy(header.signature_tail, input + 0x10C, sizeof(header.signature_tail));
	memcpy(header.id, input + 0x140, sizeof(header.id));
	memcpy(header.sha1, input + 0x12C, sizeof(header.sha1));
	memcpy(header.kirk_header, input + 0x80, 0x30);
	memcpy(header.kirk_header + 0x30, input + 0xC0, 0x10);
	memcpy(header.kirk_metadata, input + 0xB0, sizeof(header.kirk_metadata));
	memcpy(header.prx_header, input, sizeof(header.prx_header));

	result = expand_seed(expanded_seed, key, 0x43);
	if (result != 0)
		return -3;

	result = scramble_data(header.id, 0x60, 0x43);
	if (result != 0)
		return -4;

#define APPEND_HASH(data, length) \
	do { \
		memcpy(hash_input + hash_size, (data), (length)); \
		hash_size += (length); \
	} while (0)
	APPEND_HASH(header.tag, sizeof(header.tag));
	APPEND_HASH(expanded_seed, 0x10);
	APPEND_HASH(header.empty, sizeof(header.empty));
	APPEND_HASH(header.signature_tail, sizeof(header.signature_tail));
	APPEND_HASH(header.id, sizeof(header.id));
	APPEND_HASH(header.kirk_header, sizeof(header.kirk_header));
	APPEND_HASH(header.kirk_metadata, sizeof(header.kirk_metadata));
	APPEND_HASH(header.prx_header, sizeof(header.prx_header));
#undef APPEND_HASH

	result = sceKernelUtilsSha1Digest(hash_input, hash_size, digest);
	if (result < 0)
		return -5;
	if (memcmp(digest, header.sha1, sizeof(digest)) != 0) {
		memcpy(output, digest, sizeof(digest));
		memcpy(output + sizeof(digest), header.sha1, sizeof(header.sha1));
		memcpy(output + 0x28, expanded_seed, 4);
		memcpy(output + 0x2C, header.id, 4);
		return -6;
	}

	memcpy(output, input, size);
	kirk_header = output + KIRK_HEADER_OFFSET;
	memset(kirk_header, 0, KIRK_HEADER_SIZE);
	memcpy(kirk_header + 0x40, header.signature_tail, sizeof(header.signature_tail));
	memcpy(kirk_header + 0x70, header.kirk_metadata, sizeof(header.kirk_metadata));
	memcpy(kirk_header + KIRK_HEADER_SIZE, header.prx_header, sizeof(header.prx_header));

	if (decrypt_kirk_header(kirk_header, header.kirk_header,
		expanded_seed + 0x10, 0x43) < 0)
		return -7;

	*(u32 *)(kirk_header + 0x60) = 1;
	kirk_header[0x64] = 1;

	result = sceUtilsBufferCopyWithRange(output, size, kirk_header,
		size - KIRK_HEADER_OFFSET, 1);
	if (result != 0)
		return -8;

	return decrypt_size;
}

static int decrypt_type6_stack(u32 *args)
{
	return decrypt_type6((const u8 *)args[0], (u8 *)args[1], args[2], args[3]);
}

int tmDecryptPRXType6(const u8 *input, u8 *output, u32 size, u32 capacity)
{
	u32 args[4];
	int k1 = pspSdkSetK1(0);
	int result;

	args[0] = (u32)input;
	args[1] = (u32)output;
	args[2] = size;
	args[3] = capacity;
	result = sceKernelExtendKernelStack(0x4000, (void *)decrypt_type6_stack, args);
	pspSdkSetK1(k1);
	return result;
}

int module_start(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	return 0;
}
